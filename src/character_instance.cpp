#include "character_instance.h"
#include "ini.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <windows.h>

CharacterInstance::CharacterInstance()
	: parts(&cg)
{
	state.pattern = 0;
	state.frame = 0;
	state.spriteId = -1;
}

CharacterInstance::~CharacterInstance()
{
}

bool CharacterInstance::loadFromTxt(const std::string& txtPath)
{
	m_txtPath = txtPath;

	// Use existing LoadFromIni function, now with .pat support
	// LoadFromIni will load all files and set m_topHA6Path to File01 (character-specific)
	// for uni2/mbtl/dbfci/unist characters, or File00 for single-file characters
	if (!LoadFromIni(&frameData, &cg, txtPath, &m_topHA6Path, &parts, &m_patPath)) {
		return false;
	}

	// Extract character name from .txt filename
	std::filesystem::path p(txtPath);
	m_name = p.stem().string();

	// Extract HA6 paths from .txt file
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, txtPath.c_str());
	if (fileNum > 0) {
		std::string folder = txtPath.substr(0, txtPath.find_last_of("\\/"));
		m_ha6Paths.clear();

		for (int i = 0; i < fileNum; i++) {
			char ha6file[256]{};
			std::stringstream ss;
			ss << "File" << std::setfill('0') << std::setw(2) << i;
			GetPrivateProfileStringA("DataFile", ss.str().c_str(), nullptr, ha6file, 256, txtPath.c_str());

			std::string fullpath = folder + "\\" + ha6file;
			m_ha6Paths.push_back(fullpath);
		}
	}

	// Extract CG path
	char cgFile[256]{};
	GetPrivateProfileStringA("BmpcutFile", "File00", nullptr, cgFile, 256, txtPath.c_str());
	if (cgFile[0] != '\0') {
		std::string folder = txtPath.substr(0, txtPath.find_last_of("\\/"));
		m_cgPath = folder + "\\" + cgFile;
	}

	m_isModified = false;
	undoManager.markCleanState();

	// Auto-load effect character if effect.txt exists in same folder
	// (but don't try to load effect for the effect itself - prevents infinite loop)
	if (m_name != "effect") {
		loadEffectCharacter();
	}

	return true;
}

bool CharacterInstance::loadChrHA6FromTxt(const std::string& txtPath)
{
	m_txtPath = txtPath;

	// Use LoadChrHA6FromIni to load only File01 (character .ha6)
	if (!LoadChrHA6FromIni(&frameData, &cg, txtPath, &m_topHA6Path, &parts, &m_patPath)) {
		return false;
	}

	// Extract character name from .txt filename
	std::filesystem::path p(txtPath);
	m_name = p.stem().string();

	// Extract HA6 path (only File01)
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, txtPath.c_str());
	if (fileNum >= 2) {
		std::string folder = txtPath.substr(0, txtPath.find_last_of("\\/"));
		m_ha6Paths.clear();

		char ha6file[256]{};
		GetPrivateProfileStringA("DataFile", "File01", nullptr, ha6file, 256, txtPath.c_str());

		if (ha6file[0] != '\0') {
			std::string fullpath = folder + "\\" + ha6file;
			m_ha6Paths.push_back(fullpath);
		}
	}

	// Extract CG path
	char cgFile[256]{};
	GetPrivateProfileStringA("BmpcutFile", "File00", nullptr, cgFile, 256, txtPath.c_str());
	if (cgFile[0] != '\0') {
		std::string folder = txtPath.substr(0, txtPath.find_last_of("\\/"));
		m_cgPath = folder + "\\" + cgFile;
	}

	m_isModified = false;
	undoManager.markCleanState();
	return true;
}

bool CharacterInstance::loadHA6(const std::string& ha6Path, bool patch)
{
	if (!frameData.load(ha6Path.c_str(), patch)) {
		return false;
	}

	// Extract character name from filename
	std::filesystem::path p(ha6Path);
	m_name = p.stem().string();

	if (!patch) {
		m_ha6Paths.clear();
	}
	m_ha6Paths.push_back(ha6Path);
	m_topHA6Path = ha6Path;

	m_isModified = false;
	undoManager.markCleanState();
	return true;
}

bool CharacterInstance::loadCG(const std::string& cgPath)
{
	if (!cg.load(cgPath.c_str())) {
		return false;
	}

	m_cgPath = cgPath;

	// Update Parts CG reference if Parts is loaded
	if (parts.loaded) {
		parts.updateCGReference(&cg);
	}

	return true;
}

bool CharacterInstance::loadPAT(const std::string& patPath)
{
	if (!parts.Load(patPath.c_str())) {
		return false;
	}

	m_patPath = patPath;
	return true;
}

bool CharacterInstance::save()
{
	if (m_topHA6Path.empty()) {
		return false;
	}

	frameData.save(m_topHA6Path.c_str());
	m_isModified = false;
	undoManager.markCleanState();
	return true;
}

bool CharacterInstance::saveAs(const std::string& ha6Path)
{
	frameData.save(ha6Path.c_str());
	m_topHA6Path = ha6Path;

	// Update ha6 paths list
	m_ha6Paths.clear();
	m_ha6Paths.push_back(ha6Path);

	m_isModified = false;
	undoManager.markCleanState();
	return true;
}

bool CharacterInstance::saveModifiedOnly(const std::string& ha6Path)
{
	frameData.save_modified_only(ha6Path.c_str());

	// Add to .txt if we have one
	if (!m_txtPath.empty()) {
		std::filesystem::path p(ha6Path);
		AddHA6ToTxt(m_txtPath, p.filename().string());
		m_ha6Paths.push_back(ha6Path);
		m_topHA6Path = ha6Path;
	}

	m_isModified = false;
	undoManager.markCleanState();
	return true;
}

bool CharacterInstance::savePAT()
{
	if (m_patPath.empty()) {
		return false;
	}

	return parts.Save(m_patPath.c_str());
}

bool CharacterInstance::savePATAs(const std::string& patPath)
{
	if (parts.Save(patPath.c_str())) {
		m_patPath = patPath;
		return true;
	}
	return false;
}

void CharacterInstance::setName(const std::string& name)
{
	m_name = name;
}

std::string CharacterInstance::getName() const
{
	return m_name;
}

std::string CharacterInstance::getDisplayName() const
{
	if (m_name.empty()) {
		return m_isModified ? "Untitled*" : "Untitled";
	}
	return m_isModified ? m_name + "*" : m_name;
}

void CharacterInstance::markModified()
{
	m_isModified = true;
}

void CharacterInstance::clearModified()
{
	m_isModified = false;
}

bool CharacterInstance::isModified() const
{
	return m_isModified;
}

const std::vector<std::string>& CharacterInstance::getHA6Paths() const
{
	return m_ha6Paths;
}

const std::string& CharacterInstance::getCGPath() const
{
	return m_cgPath;
}

const std::string& CharacterInstance::getPATPath() const
{
	return m_patPath;
}

const std::string& CharacterInstance::getTxtPath() const
{
	return m_txtPath;
}

const std::string& CharacterInstance::getTopHA6Path() const
{
	return m_topHA6Path;
}

std::string CharacterInstance::getBaseFolder() const
{
	// Get the folder containing the .txt or .ha6 file
	std::string basePath;
	if (!m_txtPath.empty()) {
		basePath = m_txtPath;
	} else if (!m_topHA6Path.empty()) {
		basePath = m_topHA6Path;
	} else {
		return "";
	}

	size_t lastSlash = basePath.find_last_of("\\/");
	if (lastSlash != std::string::npos) {
		return basePath.substr(0, lastSlash);
	}
	return "";
}

bool CharacterInstance::isInMBAACCDataFolder() const
{
	std::string baseFolder = getBaseFolder();
	if (baseFolder.empty()) {
		return false;
	}

	// Check if path ends with /data or contains /data/
	size_t dataPos = baseFolder.find("/data");
	if (dataPos == std::string::npos) {
		dataPos = baseFolder.find("\\data");
	}

	if (dataPos == std::string::npos) {
		return false;
	}

	// Check if effect.txt exists in the same folder
	std::string effectTxtPath = baseFolder + "/effect.txt";
	std::filesystem::path effectPath(effectTxtPath);
	if (std::filesystem::exists(effectPath)) {
		return true;
	}

	// Also try backslash
	effectTxtPath = baseFolder + "\\effect.txt";
	effectPath = effectTxtPath;
	return std::filesystem::exists(effectPath);
}

CharacterInstance* CharacterInstance::getEffectCharacter() const
{
	return effectCharacter.get();
}

bool CharacterInstance::loadEffectCharacter()
{
	// Get this character's folder
	std::string baseFolder = getBaseFolder();
	if (baseFolder.empty()) {
		return false;
	}

	// Try character's folder first (MBAACC: data/chr###/effect.txt)
	std::string effectTxtPath = baseFolder + "/effect.txt";
	std::filesystem::path effectPath(effectTxtPath);

	// Try backslash if forward slash doesn't exist
	if (!std::filesystem::exists(effectPath)) {
		effectTxtPath = baseFolder + "\\effect.txt";
		effectPath = effectTxtPath;
	}

	bool useParentDirectory = false;
	std::string effectFolder = baseFolder;

	// If not found in character folder, try parent directory (UNI: data/effect.txt)
	if (!std::filesystem::exists(effectPath)) {
		std::filesystem::path charPath(baseFolder);
		std::filesystem::path parentPath = charPath.parent_path();
		effectFolder = parentPath.string();

		effectTxtPath = effectFolder + "/effect.txt";
		effectPath = effectTxtPath;

		if (!std::filesystem::exists(effectPath)) {
			effectTxtPath = effectFolder + "\\effect.txt";
			effectPath = effectTxtPath;
		}

		if (!std::filesystem::exists(effectPath)) {
			return false;  // No effect.txt found in character or parent folder
		}

		useParentDirectory = true;
		printf("[Effect] %s: Using shared effect.txt from parent directory\n", m_name.c_str());
	}

	// Create and load effect character
	auto effect = std::make_unique<CharacterInstance>();
	if (!effect->loadFromTxt(effectTxtPath)) {
		printf("[Effect] Failed to load effect.txt from: %s\n", effectFolder.c_str());
		return false;
	}

	effect->setName("effect");

	// Try loading PAT file
	// UNI uses sys_effect.pat in parent directory, MBAACC uses effect.pat in character folder
	std::string effectPatPath;
	if (useParentDirectory) {
		// UNI: data/sys_effect.pat
		effectPatPath = effectFolder + "/sys_effect.pat";
		std::filesystem::path patPath(effectPatPath);
		if (!std::filesystem::exists(patPath)) {
			effectPatPath = effectFolder + "\\sys_effect.pat";
			patPath = effectPatPath;
		}

		if (std::filesystem::exists(patPath)) {
			bool patLoaded = effect->loadPAT(effectPatPath);
			printf("[Effect] %s: sys_effect.pat %s from parent directory (%d part sets)\n",
				   m_name.c_str(),
				   patLoaded ? "loaded" : "FAILED",
				   patLoaded ? (int)effect->parts.partSets.size() : 0);
		} else {
			printf("[Effect] %s: sys_effect.pat not found in parent directory\n", m_name.c_str());
		}
	} else {
		// MBAACC: data/chr###/effect.pat
		effectPatPath = effectFolder + "/effect.pat";
		std::filesystem::path patPath(effectPatPath);
		if (!std::filesystem::exists(patPath)) {
			effectPatPath = effectFolder + "\\effect.pat";
			patPath = effectPatPath;
		}

		if (std::filesystem::exists(patPath)) {
			bool patLoaded = effect->loadPAT(effectPatPath);
			printf("[Effect] %s: effect.pat %s (%d part sets)\n",
				   m_name.c_str(),
				   patLoaded ? "loaded" : "FAILED",
				   patLoaded ? (int)effect->parts.partSets.size() : 0);
		}
	}

	effectCharacter = std::move(effect);
	printf("[Effect] %s: Loaded effect character from %s\n", m_name.c_str(), effectFolder.c_str());
	return true;
}
