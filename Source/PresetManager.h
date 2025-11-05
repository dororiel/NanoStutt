/*
  ==============================================================================

    PresetManager.h
    Created: 5 Nov 2025
    Author: NanoStutt Development

    Manages saving, loading, and organizing presets for the NanoStutt plugin.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Preset metadata structure containing information about a preset.
*/
struct PresetInfo
{
    juce::String name;
    juce::String category;
    juce::String author;
    juce::String creationDate;
    juce::String description;
    juce::File filePath;
    juce::String xmlContent;  // For BinaryData presets (stored as string)
    bool isFactory;

    PresetInfo()
        : name("Untitled"), category("User"), author(""),
          creationDate(""), description(""), isFactory(false)
    {}
};

//==============================================================================
/**
    Manages preset operations for the NanoStutt plugin.

    Features:
    - Save/load presets with metadata
    - Scan factory and user preset directories
    - XML-based preset format
    - Automatic directory creation
*/
class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager();

    //==========================================================================
    // Preset Operations

    /**
        Saves the current plugin state as a preset with the given name and metadata.

        @param presetName       Name of the preset
        @param category         Category for organization (e.g., "Rhythmic", "Glitchy")
        @param author           Author name (optional)
        @param description      Preset description (optional)
        @param overwrite        If true, overwrites existing preset with same name
        @return                 True if save was successful
    */
    bool savePreset(const juce::String& presetName,
                   const juce::String& category = "User",
                   const juce::String& author = "",
                   const juce::String& description = "",
                   bool overwrite = false);

    /**
        Loads a preset from the specified file path.

        @param filePath         Path to the preset file
        @return                 True if load was successful
    */
    bool loadPreset(const juce::File& filePath);

    /**
        Loads a preset from PresetInfo (supports both file-based and BinaryData presets).

        @param info             PresetInfo containing preset data
        @return                 True if load was successful
    */
    bool loadPreset(const PresetInfo& info);

    /**
        Deletes a user preset file.

        @param filePath         Path to the preset file to delete
        @return                 True if deletion was successful
    */
    bool deletePreset(const juce::File& filePath);

    //==========================================================================
    // Preset Discovery

    /**
        Scans and returns all available factory presets.

        @return     Array of PresetInfo for factory presets
    */
    juce::Array<PresetInfo> getFactoryPresets();

    /**
        Scans and returns all user-created presets.

        @return     Array of PresetInfo for user presets
    */
    juce::Array<PresetInfo> getUserPresets();

    /**
        Returns all presets organized by category.

        @return     StringArray of unique category names
    */
    juce::StringArray getCategories();

    /**
        Returns presets in a specific category.

        @param category     Category name to filter by
        @param includeFactory   Include factory presets
        @param includeUser      Include user presets
        @return             Array of PresetInfo matching the category
    */
    juce::Array<PresetInfo> getPresetsInCategory(const juce::String& category,
                                                 bool includeFactory = true,
                                                 bool includeUser = true);

    //==========================================================================
    // State Management

    /**
        Gets the path to the currently loaded preset (if any).

        @return     File path to current preset, or invalid File if none loaded
    */
    juce::File getCurrentPresetPath() const { return currentPresetFile; }

    /**
        Gets the name of the currently loaded preset.

        @return     Preset name, or empty string if none loaded
    */
    juce::String getCurrentPresetName() const { return currentPresetName; }

    /**
        Checks if the current state has been modified since loading a preset.

        @return     True if modified
    */
    bool isModified() const { return isStateModified; }

    /**
        Marks the current state as modified.
    */
    void setModified(bool modified) { isStateModified = modified; }

    /**
        Clears the current preset (returns to default state).
    */
    void clearCurrentPreset();

private:
    //==========================================================================
    // Helper Methods

    /**
        Gets the user presets directory, creating it if it doesn't exist.
    */
    juce::File getUserPresetsDirectory();

    /**
        Gets the factory presets directory (bundled with plugin).
    */
    juce::File getFactoryPresetsDirectory();

    /**
        Parses a preset XML file and extracts metadata.

        @param file             Preset file to parse
        @param info             PresetInfo structure to fill
        @return                 True if parsing was successful
    */
    bool parsePresetFile(const juce::File& file, PresetInfo& info);

    /**
        Parses a preset XML string and extracts metadata.
        Used for BinaryData presets stored as strings.
    */
    bool parsePresetXml(const juce::String& xmlString, PresetInfo& info);

    /**
        Creates a valid filename from a preset name.

        @param presetName       Original preset name
        @return                 Sanitized filename
    */
    juce::String createValidFilename(const juce::String& presetName);

    //==========================================================================
    // Member Variables

    juce::AudioProcessorValueTreeState& parameters;

    juce::File currentPresetFile;
    juce::String currentPresetName;
    bool isStateModified;

    // Cached preset lists (updated when scanned)
    juce::Array<PresetInfo> cachedFactoryPresets;
    juce::Array<PresetInfo> cachedUserPresets;
    juce::int64 lastScanTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
