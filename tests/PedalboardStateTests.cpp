#include "../Source/PedalboardState.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
juce::ValueTree parent(bool withBoard = true)
{
    juce::ValueTree state("AmpSuiteState");
    juce::ValueTree parameter("PARAM"); parameter.setProperty("id", "OD_ON", nullptr);
    parameter.setProperty("value", 1.0, nullptr); state.addChild(parameter, -1, nullptr);
    state.setProperty("pedalPath", "unresolved-user-pedal.nam", nullptr);
    if (withBoard) state.addChild(PedalboardState::legacy(), -1, nullptr);
    return state;
}
template<typename Mutation> void rejects(Mutation mutate)
{
    auto state = parent(); mutate(state, state.getChildWithName("PEDALBOARD"));
    const auto before = state.toXmlString();
    require(PedalboardState::validate(state).isNotEmpty(), "Invalid board must reject validation");
    require(PedalboardState::migrate(state).isNotEmpty(), "Invalid board must reject migration");
    require(state.toXmlString() == before, "Rejected board migration must not mutate the source document");
    require(!PedalboardState::equal(state, parent()) && !PedalboardState::equal(state, state), "Invalid boards must never compare as supported state");
}
}

bool runPedalboardStateTests()
{
    auto legacy = parent(false), expected = parent();
    const auto originalParameter = legacy.getChild(0).toXmlString();
    require(PedalboardState::validate(legacy).isEmpty(), "Documents without a board remain supported legacy state");
    require(PedalboardState::equal(legacy, expected), "Missing board must compare equal to its deterministic migration");
    require(PedalboardState::migrate(legacy).isEmpty(), "Legacy board migration must succeed");
    const auto migrated = legacy.toXmlString();
    require(legacy.getChild(0).toXmlString() == originalParameter && legacy["pedalPath"].toString() == "unresolved-user-pedal.nam", "Migration must preserve tone parameters and unresolved legacy asset paths");
    require(PedalboardState::migrate(legacy).isEmpty() && legacy.toXmlString() == migrated, "Board migration must be idempotent");
    require(PedalboardState::equal(legacy, expected), "Migrated board must have the original deterministic identities");
    auto board = legacy.getChildWithName("PEDALBOARD");
    require(board.getNumChildren() == 8 && board["runtime"].toString() == "legacy-fixed-v1", "Foundation must describe exactly the existing eight fixed effect bindings");
    constexpr const char* types[] {"compressor", "overdrive", "neural-pedal", "eq", "modulation", "chorus", "delay", "reverb"};
    for (int i = 0; i < 8; ++i)
        require(board.getChild(i)["id"].toString() == "legacy." + juce::String(types[i]), "Legacy block identities must be deterministic and kind-qualified");

    // User/session IDs are persistent identities rather than array positions.
    auto distinct = parent(); distinct.getChildWithName("PEDALBOARD").getChild(3).setProperty("id", "board-eq_391.A", nullptr);
    const auto distinctXml = distinct.toXmlString();
    require(PedalboardState::migrate(distinct).isEmpty() && distinct.toXmlString() == distinctXml, "Valid saved block IDs must survive migration unchanged");
    require(!PedalboardState::equal(distinct, expected), "Different stable block identities must mark a board as changed");
    const auto xml = juce::XmlDocument::parse(distinctXml);
    require(xml != nullptr, "Board XML must parse after serialization");
    const auto roundTrip = juce::ValueTree::fromXml(*xml);
    require(PedalboardState::validate(roundTrip).isEmpty() && PedalboardState::equal(distinct, roundTrip), "Board XML must validate and preserve identities despite XML numeric attribute types");
    require(PedalboardState::validate(juce::ValueTree()).isNotEmpty(), "An invalid parent tree must reject safely");

    rejects([](auto& state, auto board) { state.addChild(board.createCopy(), -1, nullptr); });
    rejects([](auto&, auto board) { board.setProperty("version", 2, nullptr); });
    rejects([](auto&, auto board) { board.setProperty("version", static_cast<juce::int64>(4294967297LL), nullptr); });
    rejects([](auto&, auto board) { board.setProperty("version", true, nullptr); });
    rejects([](auto&, auto board) { board.setProperty("version", 1.0, nullptr); });
    rejects([](auto&, auto board) { board.setProperty("version", "01", nullptr); });
    rejects([](auto&, auto board) { board.setProperty("runtime", "serial-v2", nullptr); });
    rejects([](auto&, auto board) { board.setProperty("bypass", true, nullptr); });
    rejects([](auto&, auto board) { board.removeProperty("version", nullptr); });
    rejects([](auto&, auto board) { board.removeChild(7, nullptr); });
    rejects([](auto&, auto board) { board.addChild(board.getChild(7).createCopy(), -1, nullptr); });
    rejects([](auto&, auto board) { board.moveChild(0, 1, nullptr); });
    rejects([](auto&, auto board) { board.getChild(1).setProperty("type", "future-fuzz", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("anchor", "pre-amp", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("automationSlot", 1, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("automationSlot", static_cast<juce::int64>(4294967296LL), nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("automationSlot", false, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("automationSlot", 0.0, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("automationSlot", "0.0", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", 1.0, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", std::numeric_limits<double>::quiet_NaN(), nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", std::numeric_limits<double>::infinity(), nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", "0junk", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", "1e-999", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", " 0", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("trimDb", false, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("id", "", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("id", juce::String::repeatedString("x", 65), nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("id", "spaces are not identities", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("id", board.getChild(0)["id"], nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("id", 3, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("bypass", false, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("EQ_FOCUS", 5, nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).addChild(juce::ValueTree("PARAM"), -1, nullptr); });
    rejects([](auto&, auto board) { board.getChild(2).setProperty("assetKey", "model", nullptr); });
    rejects([](auto&, auto board) { board.getChild(2).removeProperty("assetKey", nullptr); });
    rejects([](auto&, auto board) { board.getChild(2).setProperty("assetId", "pedal:untracked", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("assetKey", "pedal", nullptr); });
    rejects([](auto&, auto board) { board.getChild(3).setProperty("oversized", juce::String::repeatedString("x", PedalboardState::maximumBytes), nullptr); });
    std::cout << "Pedalboard foundation migration, identity, XML round-trip and strict rejection checks passed\n";
    return true;
}
