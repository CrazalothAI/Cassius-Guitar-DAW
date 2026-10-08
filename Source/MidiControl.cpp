#include "MidiControl.h"

MidiControl::MidiControl() : Thread("Cassian MIDI control")
{ for (int i = 0; i < 8; ++i) mappings[static_cast<size_t>(i)].number = 16 + i; }
void MidiControl::start(Action callback) { action = std::move(callback); startThread(); }
void MidiControl::shutdown() { signalThreadShouldExit(); notify(); stopThread(-1); }
const MidiControl::BoardTarget* MidiControl::boardTarget(const juce::String& action)
{
    // Kind-qualified automation slots remain stable when cards change order.
    static constexpr BoardTarget targets[] {
        {"distortion1", "distortion", 0, nullptr}, {"distortion2", "distortion", 1, nullptr},
        {"plate1", "plate", 0, nullptr}, {"plate2", "plate", 1, nullptr},
        {"spring1", "spring", 0, nullptr}, {"spring2", "spring", 1, nullptr},
        {"dist-drive1", "distortion", 0, "DIST_DRIVE"}, {"dist-drive2", "distortion", 1, "DIST_DRIVE"},
        {"plate-mix1", "plate", 0, "PLATE_MIX"}, {"plate-mix2", "plate", 1, "PLATE_MIX"},
        {"spring-mix1", "spring", 0, "SPRING_MIX"}, {"spring-mix2", "spring", 1, "SPRING_MIX"}
    };
    for (const auto& target : targets) if (action == target.action) return &target;
    return nullptr;
}
bool MidiControl::expression(const juce::String& s) {
    const auto* target = boardTarget(s);
    return s == "master" || s == "drive" || s == "reverb" || s == "delay" || s == "wah1" || s == "wah2" || (target != nullptr && target->control != nullptr);
}
juce::var MidiControl::describe(const Mapping& m)
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty("type", m.type); o->setProperty("channel", m.channel); o->setProperty("number", m.number);
    o->setProperty("action", m.action); o->setProperty("rig", m.rig); o->setProperty("scene", m.scene); o->setProperty("inverted", m.inverted); return juce::var(o.release());
}
juce::String MidiControl::parse(const juce::var& v, Mapping& m)
{
    const auto integer = [&](const char* key, int lo, int hi) {
        const auto x = v[key]; return (x.isInt() || x.isInt64()) && static_cast<juce::int64>(x) >= lo && static_cast<juce::int64>(x) <= hi;
    };
    if (!v.isObject() || !v["type"].isString() || !v["action"].isString() || !integer("channel", 0, 16) || !integer("number", 0, 127)
        || (v.hasProperty("inverted") && !v["inverted"].isBool()) || (v.hasProperty("rig") && !v["rig"].isString()) || (v.hasProperty("scene") && !integer("scene", 0, 3))) return "Invalid MIDI assignment.";
    m.scene = v.hasProperty("scene") ? static_cast<int>(v["scene"]) : 0;
    m.type = v["type"].toString(); m.action = v["action"].toString(); m.channel = v["channel"]; m.number = v["number"]; m.rig = v["rig"].toString(); m.inverted = v["inverted"];
    if ((m.type != "cc" && m.type != "pc") || (!juce::StringArray {"none", "rig", "overdrive", "pedal", "eq", "gate", "metronome", "modulation", "scene"}.contains(m.action) && !expression(m.action) && boardTarget(m.action) == nullptr)) return "Unsupported MIDI assignment.";
    if (m.type == "pc" && expression(m.action)) return "Expression control needs a CC message.";
    if (m.rig.length() > 128 || (m.action == "rig" && m.rig.isEmpty())) return "Choose a saved rig for this assignment.";
    return {};
}
bool MidiControl::conflicts(const std::array<Mapping, 8>& rows)
{
    for (size_t a = 0; a < rows.size(); ++a) for (size_t b = a + 1; b < rows.size(); ++b)
        if (rows[a].action != "none" && rows[b].action != "none" && rows[a].type == rows[b].type && rows[a].number == rows[b].number
            && (rows[a].channel == 0 || rows[b].channel == 0 || rows[a].channel == rows[b].channel)) return true;
    return false;
}
void MidiControl::enable(bool on)
{ const juce::ScopedLock guard(lock); enabled.store(on); learning.store(-1); ++epoch; ++revision; error.clear(); }
juce::String MidiControl::learn(int slot)
{
    if (slot < -1 || slot >= 8) return "Choose one of the eight MIDI assignments.";
    const juce::ScopedLock guard(lock); learning.store(slot); ++epoch; error.clear(); return {};
}
juce::String MidiControl::setMapping(int slot, const juce::var& v)
{
    if (slot < 0 || slot >= 8) return "Choose one of the eight MIDI assignments.";
    Mapping next; const auto failure = parse(v, next); if (failure.isNotEmpty()) return failure;
    const juce::ScopedLock guard(lock); auto rows = mappings; rows[static_cast<size_t>(slot)] = next;
    if (conflicts(rows)) return "This message overlaps another assignment; choose a different number or channel.";
    mappings = std::move(rows); learning.store(-1); ++epoch; ++revision; error.clear(); return {};
}
juce::var MidiControl::configuration()
{
    const juce::ScopedLock guard(lock); auto o = std::make_unique<juce::DynamicObject>(); juce::Array<juce::var> rows;
    for (const auto& m : mappings) rows.add(describe(m));
    o->setProperty("version", 1); o->setProperty("enabled", enabled.load()); o->setProperty("mappings", rows); return juce::var(o.release());
}
juce::String MidiControl::restore(const juce::var& v)
{
    std::array<Mapping, 8> rows; for (int i = 0; i < 8; ++i) rows[static_cast<size_t>(i)].number = 16 + i;
    juce::String failure; bool on = false;
    if (!v.isVoid()) {
        if (!v.isObject() || !v["version"].isInt() || static_cast<int>(v["version"]) != 1 || !v["enabled"].isBool() || !v["mappings"].isArray() || v["mappings"].size() != 8) failure = "Invalid saved MIDI configuration; mapping is disabled.";
        else {
            for (int i = 0; i < 8 && failure.isEmpty(); ++i) failure = parse(v["mappings"][i], rows[static_cast<size_t>(i)]);
            if (failure.isEmpty() && conflicts(rows)) failure = "Saved MIDI assignments overlap; mapping is disabled.";
            if (failure.isEmpty()) on = v["enabled"];
        }
    }
    const juce::ScopedLock guard(lock);
    if (failure.isEmpty()) mappings = std::move(rows);
    else { for (int i = 0; i < 8; ++i) { mappings[static_cast<size_t>(i)] = Mapping(); mappings[static_cast<size_t>(i)].number = 16 + i; } }
    enabled.store(on); learning.store(-1); ++epoch; ++revision; error = failure; return failure;
}
void MidiControl::receive(const juce::MidiBuffer& midi)
{
    int seen = 0;
    for (const auto metadata : midi) {
        if (++seen > 512) { ++dropped; break; }
        const auto* data = metadata.data; const int size = metadata.numBytes;
        if (size < 2) continue;
        const unsigned kind = data[0] & 0xf0, channel = data[0] & 0xf;
        if (kind != 0xb0 && kind != 0xc0) continue;
        if (data[1] > 127 || (kind == 0xb0 && (size < 3 || data[2] > 127))) continue;
        const unsigned number = data[1], value = kind == 0xb0 ? data[2] : 127;
        const auto index = channel * 128 + number;
        const bool edge = kind == 0xc0 || (value >= 64 && !pressed[index]);
        if (kind == 0xb0) pressed[index] = value >= 64;
        const unsigned packed = (kind == 0xb0 ? 1u : 2u) | (channel << 2) | (number << 6) | (value << 13) | (static_cast<unsigned>(edge) << 20);
        lastInput.store(packed);
        if (!enabled.load() && learning.load() < 0) continue;
        int a, na, b, nb; fifo.prepareToWrite(1, a, na, b, nb);
        if (na + nb == 0) { ++dropped; continue; }
        events[static_cast<size_t>(na > 0 ? a : b)] = {epoch.load(), packed}; fifo.finishedWrite(1);
    }
}
void MidiControl::run()
{
    while (!threadShouldExit()) {
        int a, na, b, nb; fifo.prepareToRead(fifo.getNumReady(), a, na, b, nb);
        const auto consume = [&](int offset, int n) {
            for (int i = 0; i < n && !threadShouldExit(); ++i) {
                const auto event = events[static_cast<size_t>(offset + i)];
                const unsigned kind = event.packed & 3, channel = ((event.packed >> 2) & 15) + 1, number = (event.packed >> 6) & 127, value = (event.packed >> 13) & 127;
                const bool edge = (event.packed & (1u << 20)) != 0;
                std::array<Mapping, 8> rows;
                { const juce::ScopedLock guard(lock);
                  if (event.epoch != epoch.load()) continue;
                  if (learning.load() >= 0) {
                      const int slot = learning.load(); auto next = mappings[static_cast<size_t>(slot)];
                      next.type = kind == 1 ? "cc" : "pc"; next.channel = static_cast<int>(channel); next.number = static_cast<int>(number);
                      auto candidate = mappings; candidate[static_cast<size_t>(slot)] = next;
                      if ((kind == 2 && expression(next.action)) || conflicts(candidate)) { error = "Learned message conflicts with this action or another assignment. Try another message."; continue; }
                      mappings = std::move(candidate); learning.store(-1); ++epoch; ++revision; error.clear(); continue; // Learning never executes the action.
                  }
                  if (!enabled.load()) continue; rows = mappings; }
                for (const auto& m : rows) {
                    if (event.epoch != epoch.load() || !enabled.load()) break;
                    if (m.action == "none" || (m.type == "cc" ? 1u : 2u) != kind || m.number != static_cast<int>(number) || (m.channel != 0 && m.channel != static_cast<int>(channel)) || (!expression(m.action) && !edge)) continue;
                    juce::String failure;
                    try { failure = action ? action(m, static_cast<int>(value)) : juce::String("MIDI control is unavailable."); }
                    catch (...) { failure = "MIDI action failed; try again."; }
                    const juce::ScopedLock guard(lock); error = failure; lastAction = failure.isEmpty() ? m.action : juce::String();
                }
            }
        };
        consume(a, na); consume(b, nb); fifo.finishedRead(na + nb); wait(10);
    }
}
juce::var MidiControl::status()
{
    auto o = std::make_unique<juce::DynamicObject>(); o->setProperty("config", configuration());
    { const juce::ScopedLock guard(lock); o->setProperty("error", error); o->setProperty("lastAction", lastAction); }
    o->setProperty("learning", learning.load()); o->setProperty("revision", static_cast<int>(revision.load())); o->setProperty("dropped", static_cast<int>(dropped.load()));
    const auto packed = lastInput.load(); juce::String last;
    if (packed != 0) last = juce::String((packed & 3) == 1 ? "CC " : "PC ") + juce::String((packed >> 6) & 127) + " · channel " + juce::String(((packed >> 2) & 15) + 1) + " · value " + juce::String((packed >> 13) & 127);
    o->setProperty("lastInput", last); return juce::var(o.release());
}
