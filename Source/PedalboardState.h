#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <initializer_list>

// State foundation for the existing fixed DSP. No parsing or tree access occurs
// in the audio callback, and this version does not enable arbitrary routing.
class PedalboardState
{
public:
    static constexpr int maximumBytes = 16 * 1024;
    static constexpr int plannedAutomationSlots = 16;

    static juce::ValueTree legacy()
    {
        juce::ValueTree board("PEDALBOARD");
        board.setProperty("version", 1, nullptr);
        board.setProperty("runtime", "legacy-fixed-v1", nullptr);
        for (const auto& binding : bindings) {
            juce::ValueTree block("BLOCK");
            block.setProperty("id", "legacy." + juce::String(binding.type), nullptr);
            block.setProperty("type", binding.type, nullptr);
            block.setProperty("automationSlot", 0, nullptr);
            block.setProperty("anchor", binding.anchor, nullptr);
            block.setProperty("trimDb", 0.0, nullptr);
            if (juce::String(binding.type) == "neural-pedal") block.setProperty("assetKey", "pedal", nullptr);
            board.addChild(block, -1, nullptr);
        }
        return board;
    }

    // A missing child is the supported legacy representation. A present child
    // must be fully understood before any parameters or loaded assets change.
    static juce::String validate(const juce::ValueTree& parent)
    {
        if (!parent.isValid()) return "Invalid pedalboard parent state.";
        juce::ValueTree board;
        for (const auto& child : parent) if (child.hasType("PEDALBOARD")) {
            if (board.isValid()) return "Duplicate pedalboard state.";
            board = child;
        }
        if (!board.isValid()) return {};
        if (board.toXmlString().getNumBytesAsUTF8() > maximumBytes) return "Pedalboard state is too large.";
        if (!properties(board, {"version", "runtime"}) || !integer(board["version"], 1)
            || !board["runtime"].isString() || board["runtime"].toString() != "legacy-fixed-v1")
            return "Unsupported pedalboard version or runtime.";
        if (board.getNumChildren() != static_cast<int>(bindings.size())) return "Incomplete or unsupported pedalboard.";
        juce::StringArray identities;
        for (size_t i = 0; i < bindings.size(); ++i) {
            const auto block = board.getChild(static_cast<int>(i));
            const auto& binding = bindings[i];
            const bool neural = juce::String(binding.type) == "neural-pedal";
            if (!block.hasType("BLOCK") || block.getNumChildren() != 0
                || !(neural ? properties(block, {"id", "type", "automationSlot", "anchor", "trimDb", "assetKey"})
                            : properties(block, {"id", "type", "automationSlot", "anchor", "trimDb"})))
                return "Invalid pedalboard block structure.";
            const auto id = block["id"].toString();
            if (!block["id"].isString() || id.isEmpty() || id.length() > 64
                || id.removeCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-").isNotEmpty()
                || identities.contains(id)) return "Invalid or duplicate pedalboard block identity.";
            identities.add(id);
            if (!block["type"].isString() || block["type"].toString() != binding.type
                || !block["anchor"].isString() || block["anchor"].toString() != binding.anchor
                || !integer(block["automationSlot"], 0)) return "Unsupported pedalboard order or automation binding.";
            if (!zero(block["trimDb"])) return "Pedalboard output trim is not supported by this runtime.";
            if (neural && (!block["assetKey"].isString() || block["assetKey"].toString() != "pedal"))
                return "Unsupported neural pedal asset reference.";
        }
        return {};
    }

    static juce::String migrate(juce::ValueTree& parent)
    {
        if (const auto error = validate(parent); error.isNotEmpty()) return error;
        if (!parent.getChildWithName("PEDALBOARD").isValid()) parent.addChild(legacy(), -1, nullptr);
        return {};
    }

    // Compare valid effective boards, including a document from before this
    // schema. XML stores numbers as strings, so compare validated bindings and
    // identities rather than the in-memory var representation of zero or one.
    static bool equal(const juce::ValueTree& a, const juce::ValueTree& b)
    {
        if (validate(a).isNotEmpty() || validate(b).isNotEmpty()) return false;
        const auto left = effective(a), right = effective(b);
        for (int i = 0; i < left.getNumChildren(); ++i)
            if (left.getChild(i)["id"].toString() != right.getChild(i)["id"].toString()) return false;
        return true;
    }

private:
    struct Binding { const char* type; const char* anchor; };
    inline static constexpr std::array bindings {
        Binding {"compressor", "compressor-mode"}, Binding {"overdrive", "pre-amp"},
        Binding {"neural-pedal", "amp-pedal"}, Binding {"eq", "post-eq"},
        Binding {"modulation", "post-modulation"}, Binding {"chorus", "post-chorus"},
        Binding {"delay", "post-delay"}, Binding {"reverb", "post-reverb"}
    };

    static bool properties(const juce::ValueTree& tree, std::initializer_list<const char*> keys)
    {
        if (tree.getNumProperties() != static_cast<int>(keys.size())) return false;
        for (const auto* key : keys) if (!tree.hasProperty(key)) return false;
        return true;
    }
    static bool integer(const juce::var& value, int expected)
    {
        if (value.isInt() || value.isInt64()) return static_cast<juce::int64>(value) == expected;
        // ValueTree XML attributes are strings on read. Accept the exact integer
        // spelling emitted by createXml, never a bool, fraction, or wrapping int.
        return value.isString() && value.toString() == juce::String(expected);
    }
    static bool zero(const juce::var& value)
    {
        if (value.isInt() || value.isInt64() || value.isDouble()) {
            const auto x = static_cast<double>(value);
            return std::isfinite(x) && x == 0;
        }
        if (!value.isString()) return false;
        const auto text = value.toString();
        if (text.isEmpty() || text.length() > 32 || text.trim() != text) return false;
        const auto utf8 = text.toStdString(); char* end = nullptr; errno = 0;
        const auto x = std::strtod(utf8.c_str(), &end);
        return end != utf8.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(x) && x == 0;
    }
    static juce::ValueTree effective(const juce::ValueTree& parent)
    {
        const auto board = parent.getChildWithName("PEDALBOARD"); return board.isValid() ? board : legacy();
    }
};
