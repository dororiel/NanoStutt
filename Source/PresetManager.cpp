/*
  ==============================================================================

    PresetManager.cpp
    Created: 5 Nov 2025
    Author: NanoStutt Development

  ==============================================================================
*/

#include "PresetManager.h"

//==============================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : parameters(apvts),
      isStateModified(false),
      lastScanTime(0)
{
    // Ensure user presets directory exists
    getUserPresetsDirectory();
}

PresetManager::~PresetManager()
{
}

//==============================================================================
bool PresetManager::savePreset(const juce::String& presetName,
                               const juce::String& category,
                               const juce::String& author,
                               const juce::String& description,
                               bool overwrite)
{
    if (presetName.isEmpty())
        return false;

    // Get user presets directory
    juce::File userDir = getUserPresetsDirectory();
    if (!userDir.exists())
    {
        DBG("Failed to create user presets directory");
        return false;
    }

    // Create category subdirectory if it doesn't exist
    juce::File categoryDir = userDir.getChildFile(category);
    if (!categoryDir.exists())
        categoryDir.createDirectory();

    // Create filename from preset name
    juce::String filename = createValidFilename(presetName) + ".xml";
    juce::File presetFile = categoryDir.getChildFile(filename);

    // Check if file exists and we're not allowed to overwrite
    if (presetFile.existsAsFile() && !overwrite)
    {
        DBG("Preset file already exists and overwrite is false");
        return false;
    }

    // Create XML structure
    juce::XmlElement root("NANOSTUTT_PRESET");

    // Add metadata
    auto* metadata = root.createNewChildElement("METADATA");
    metadata->setAttribute("name", presetName);
    metadata->setAttribute("category", category);
    metadata->setAttribute("author", author.isNotEmpty() ? author : "Unknown");
    metadata->setAttribute("creationDate", juce::Time::getCurrentTime().toString(true, true));
    metadata->setAttribute("description", description);

    // Add plugin state (all parameters)
    auto state = parameters.copyState();
    auto stateXml = state.createXml();

    // Exclude autoStutterEnabled parameter from preset
    if (stateXml != nullptr)
    {
        // Remove autoStutterEnabled from state
        for (auto* child : stateXml->getChildIterator())
        {
            if (child->hasAttribute("id") && child->getStringAttribute("id") == "autoStutterEnabled")
            {
                stateXml->removeChildElement(child, true);
                break;
            }
        }

        root.addChildElement(stateXml.release());
    }

    // Write to file
    bool success = root.writeTo(presetFile);

    if (success)
    {
        currentPresetFile = presetFile;
        currentPresetName = presetName;
        isStateModified = false;

        // Clear cached presets to force rescan
        cachedUserPresets.clear();
        lastScanTime = 0;

        DBG("Preset saved successfully: " + presetFile.getFullPathName());
    }
    else
    {
        DBG("Failed to write preset file: " + presetFile.getFullPathName());
    }

    return success;
}

bool PresetManager::loadPreset(const juce::File& filePath)
{
    if (!filePath.existsAsFile())
    {
        DBG("Preset file does not exist: " + filePath.getFullPathName());
        return false;
    }

    // Parse XML file
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(filePath));

    if (xml == nullptr || !xml->hasTagName("NANOSTUTT_PRESET"))
    {
        DBG("Invalid preset file format");
        return false;
    }

    // Extract metadata
    auto* metadata = xml->getChildByName("METADATA");
    if (metadata != nullptr)
    {
        currentPresetName = metadata->getStringAttribute("name", "Untitled");
    }

    // Find the state element (should be the second child, after METADATA)
    juce::XmlElement* stateXml = nullptr;
    for (auto* child : xml->getChildIterator())
    {
        if (child->hasTagName(parameters.state.getType()))
        {
            stateXml = child;
            break;
        }
    }

    if (stateXml == nullptr)
    {
        DBG("No state data found in preset file");
        return false;
    }

    // Store current autoStutterEnabled value
    bool currentAutoStutterEnabled = parameters.getRawParameterValue("autoStutterEnabled")->load() > 0.5f;

    // Create ValueTree from XML and restore state
    juce::ValueTree newState = juce::ValueTree::fromXml(*stateXml);
    if (newState.isValid())
    {
        parameters.replaceState(newState);

        // Restore the autoStutterEnabled parameter (don't let preset override it)
        auto* param = parameters.getParameter("autoStutterEnabled");
        if (param != nullptr)
            param->setValueNotifyingHost(currentAutoStutterEnabled ? 1.0f : 0.0f);

        currentPresetFile = filePath;
        isStateModified = false;

        DBG("Preset loaded successfully: " + filePath.getFullPathName());
        return true;
    }

    DBG("Failed to create ValueTree from preset state");
    return false;
}

bool PresetManager::loadPreset(const PresetInfo& info)
{
    std::unique_ptr<juce::XmlElement> xml;

    // Check if this is a BinaryData preset (has xmlContent) or file-based preset
    if (info.xmlContent.isNotEmpty())
    {
        // BinaryData preset - parse from string
        xml = juce::XmlDocument::parse(info.xmlContent);
    }
    else if (info.filePath.existsAsFile())
    {
        // File-based preset - parse from file
        xml = juce::XmlDocument::parse(info.filePath);
    }
    else
    {
        DBG("Preset has no valid xmlContent or filePath");
        return false;
    }

    if (xml == nullptr || !xml->hasTagName("NANOSTUTT_PRESET"))
    {
        DBG("Invalid preset format");
        return false;
    }

    // Extract metadata
    auto* metadata = xml->getChildByName("METADATA");
    if (metadata != nullptr)
    {
        currentPresetName = metadata->getStringAttribute("name", "Untitled");
    }

    // Find the state element (should be the second child, after METADATA)
    juce::XmlElement* stateXml = nullptr;
    for (auto* child : xml->getChildIterator())
    {
        if (child->hasTagName(parameters.state.getType()))
        {
            stateXml = child;
            break;
        }
    }

    if (stateXml == nullptr)
    {
        DBG("No state data found in preset");
        return false;
    }

    // Store current autoStutterEnabled value
    bool currentAutoStutterEnabled = parameters.getRawParameterValue("autoStutterEnabled")->load() > 0.5f;

    // Create ValueTree from XML and restore state
    juce::ValueTree newState = juce::ValueTree::fromXml(*stateXml);
    if (newState.isValid())
    {
        parameters.replaceState(newState);

        // Restore the autoStutterEnabled parameter (don't let preset override it)
        auto* param = parameters.getParameter("autoStutterEnabled");
        if (param != nullptr)
            param->setValueNotifyingHost(currentAutoStutterEnabled ? 1.0f : 0.0f);

        currentPresetFile = info.filePath;
        isStateModified = false;

        DBG("Preset loaded successfully: " + info.name);
        return true;
    }

    DBG("Failed to create ValueTree from preset state");
    return false;
}

bool PresetManager::deletePreset(const juce::File& filePath)
{
    if (!filePath.existsAsFile())
        return false;

    // Only allow deletion of user presets (safety check)
    juce::File userDir = getUserPresetsDirectory();
    if (!filePath.isAChildOf(userDir))
    {
        DBG("Cannot delete factory presets");
        return false;
    }

    bool success = filePath.deleteFile();

    if (success)
    {
        // Clear cached presets to force rescan
        cachedUserPresets.clear();
        lastScanTime = 0;

        // Clear current preset if we just deleted it
        if (currentPresetFile == filePath)
            clearCurrentPreset();

        DBG("Preset deleted: " + filePath.getFullPathName());
    }

    return success;
}

//==============================================================================
juce::Array<PresetInfo> PresetManager::getFactoryPresets()
{
    // Return cached results if available
    if (!cachedFactoryPresets.isEmpty())
        return cachedFactoryPresets;

    juce::Array<PresetInfo> presets;

    // Load presets from BinaryData (embedded in the plugin binary)
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* resourceName = BinaryData::namedResourceList[i];
        int dataSize = 0;
        const char* xmlData = BinaryData::getNamedResource(resourceName, dataSize);

        if (xmlData != nullptr && dataSize > 0)
        {
            // Convert to JUCE string
            juce::String xmlString(xmlData, dataSize);

            // Parse the XML
            PresetInfo info;
            info.isFactory = true;

            if (parsePresetXml(xmlString, info))
            {
                presets.add(info);
            }
        }
    }

    // Cache results
    cachedFactoryPresets = presets;

    DBG("Loaded " + juce::String(presets.size()) + " factory presets from BinaryData");

    return presets;
}

juce::Array<PresetInfo> PresetManager::getUserPresets()
{
    juce::int64 currentTime = juce::Time::currentTimeMillis();

    // Return cached results if less than 1 second old
    if (!cachedUserPresets.isEmpty() && (currentTime - lastScanTime) < 1000)
        return cachedUserPresets;

    juce::Array<PresetInfo> presets;
    juce::File userDir = getUserPresetsDirectory();

    if (!userDir.exists())
        return presets;

    // Recursively find all .xml files
    juce::Array<juce::File> presetFiles;
    userDir.findChildFiles(presetFiles, juce::File::findFiles, true, "*.xml");

    for (const auto& file : presetFiles)
    {
        PresetInfo info;
        info.isFactory = false;

        if (parsePresetFile(file, info))
            presets.add(info);
    }

    // Cache results
    cachedUserPresets = presets;
    lastScanTime = currentTime;

    return presets;
}

juce::StringArray PresetManager::getCategories()
{
    juce::StringArray categories;

    // Get all presets
    auto factoryPresets = getFactoryPresets();
    auto userPresets = getUserPresets();

    // Extract unique categories
    for (const auto& preset : factoryPresets)
    {
        if (!categories.contains(preset.category))
            categories.add(preset.category);
    }

    for (const auto& preset : userPresets)
    {
        if (!categories.contains(preset.category))
            categories.add(preset.category);
    }

    categories.sort(true); // Case-insensitive sort

    return categories;
}

juce::Array<PresetInfo> PresetManager::getPresetsInCategory(const juce::String& category,
                                                           bool includeFactory,
                                                           bool includeUser)
{
    juce::Array<PresetInfo> result;

    if (includeFactory)
    {
        auto factoryPresets = getFactoryPresets();
        for (const auto& preset : factoryPresets)
        {
            if (preset.category == category)
                result.add(preset);
        }
    }

    if (includeUser)
    {
        auto userPresets = getUserPresets();
        for (const auto& preset : userPresets)
        {
            if (preset.category == category)
                result.add(preset);
        }
    }

    return result;
}

//==============================================================================
void PresetManager::clearCurrentPreset()
{
    currentPresetFile = juce::File();
    currentPresetName = "";
    isStateModified = false;
}

//==============================================================================
juce::File PresetManager::getUserPresetsDirectory()
{
    // Get user's application data directory
    // macOS: ~/Library/Audio/Presets/NanoStutt/
    // Windows: %APPDATA%\NanoStutt\Presets\

    juce::File presetDir;

#if JUCE_MAC
    presetDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                    .getChildFile("Audio")
                    .getChildFile("Presets")
                    .getChildFile("NanoStutt");
#elif JUCE_WINDOWS
    presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("NanoStutt")
                    .getChildFile("Presets");
#else
    presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(".nanostutt")
                    .getChildFile("presets");
#endif

    // Create directory if it doesn't exist
    if (!presetDir.exists())
        presetDir.createDirectory();

    return presetDir;
}

juce::File PresetManager::getFactoryPresetsDirectory()
{
    // Factory presets are bundled with the plugin
    // For now, return an empty directory (will be populated when we embed presets)
    // In the future, this will use BinaryData to access embedded presets

    // Placeholder: Check in the plugin bundle's Resources folder
#if JUCE_MAC
    juce::File bundleDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    return bundleDir.getChildFile("Contents").getChildFile("Resources").getChildFile("Presets");
#elif JUCE_WINDOWS
    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return exeDir.getChildFile("Presets");
#else
    return juce::File();
#endif
}

bool PresetManager::parsePresetFile(const juce::File& file, PresetInfo& info)
{
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));

    if (xml == nullptr || !xml->hasTagName("NANOSTUTT_PRESET"))
        return false;

    // Extract metadata
    auto* metadata = xml->getChildByName("METADATA");
    if (metadata == nullptr)
        return false;

    info.name = metadata->getStringAttribute("name", "Untitled");
    info.category = metadata->getStringAttribute("category", "Uncategorized");
    info.author = metadata->getStringAttribute("author", "Unknown");
    info.creationDate = metadata->getStringAttribute("creationDate", "");
    info.description = metadata->getStringAttribute("description", "");
    info.filePath = file;

    return true;
}

bool PresetManager::parsePresetXml(const juce::String& xmlString, PresetInfo& info)
{
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(xmlString));

    if (xml == nullptr || !xml->hasTagName("NANOSTUTT_PRESET"))
        return false;

    // Extract metadata
    auto* metadata = xml->getChildByName("METADATA");
    if (metadata == nullptr)
        return false;

    info.name = metadata->getStringAttribute("name", "Untitled");
    info.category = metadata->getStringAttribute("category", "Uncategorized");
    info.author = metadata->getStringAttribute("author", "Unknown");
    info.creationDate = metadata->getStringAttribute("creationDate", "");
    info.description = metadata->getStringAttribute("description", "");
    info.xmlContent = xmlString; // Store the XML for later loading

    return true;
}

juce::String PresetManager::createValidFilename(const juce::String& presetName)
{
    // Remove invalid filename characters and replace with underscores
    juce::String filename = presetName;

    // Characters not allowed in filenames (cross-platform)
    const juce::String invalidChars = "<>:\"/\\|?*";

    for (int i = 0; i < invalidChars.length(); ++i)
    {
        filename = filename.replace(juce::String::charToString(invalidChars[i]), "_");
    }

    // Trim leading/trailing spaces
    filename = filename.trim();

    // Ensure filename is not empty
    if (filename.isEmpty())
        filename = "Untitled";

    return filename;
}
