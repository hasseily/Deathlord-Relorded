//
//  MemoryLoader.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 17/01/2024.
//

#include "MemoryLoader.h"

#include <fstream>
#include <iostream>
#include "../MemoryManager.h"
#include "ImGuiFileDialog.h"
#include "../LogTextManager.h"
#include <locale.h>
#include <sstream>

namespace sdd {

	bool MemoryLoad(const std::string &filePath, uint32_t position, bool bAuxBank, size_t fileSize) {
		bool res = false;

		uint8_t* pMem;
		if (bAuxBank)
			pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + position;
		else
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + position;

		std::ifstream file(filePath, std::ios::binary);
		if (file) {
			// Move to the end to get the file size
			if (fileSize == 0) {
				file.seekg(0, std::ios::end);
				fileSize = file.tellg();
			}

			if (fileSize == 0) {
				std::cerr << "Error: File size is zero." << std::endl;
			} else {
				if ((position + fileSize) > (_A2_MEMORY_SHADOW_END))
					fileSize = _A2_MEMORY_SHADOW_END - position;
				file.seekg(0, std::ios::beg); // Go back to the start of the file
				file.read(reinterpret_cast<char*>(pMem), fileSize);
				res = true;
			}
			file.close();
		} else {
			// Handle the error: file could not be opened
			std::cerr << "Error: Unable to open file." << std::endl;
		}
		return res;
	}

	bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank, std::string& path) {
		setlocale(LC_ALL, ".UTF8");
		bool res = false;
		if (ImGui::Button("Load File"))
		{
			if (ImGuiFileDialog::Instance()->IsOpened())
				ImGuiFileDialog::Instance()->Close();
			ImGui::SetNextWindowSize(ImVec2(800, 400));
			IGFD::FileDialogConfig config;
			config.path = (path.empty() ? "." : path);
			ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
													".bin,.txt,.lgr,.dgr,.hgr,.dhr,.shr, #C10000, #C10002", config);
		}

		// Display the file dialog
		if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
			// Check if a file was selected
			if (ImGuiFileDialog::Instance()->IsOk()) {
				std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
				path = ImGuiFileDialog::Instance()->GetCurrentPath();
				if (filePath.length() >= 4) {
					std::string extension = ImGuiFileDialog::Instance()->GetCurrentFilter();
					if (extension == ".lgr")
						res = MemoryLoadLGR(filePath);
					else if (extension == ".dgr")
						res =  MemoryLoadDGR(filePath);
					else if (extension == ".hgr")
						res = MemoryLoadHGR(filePath);
					else if (extension == ".dhr")
						res = MemoryLoadDHR(filePath);
					else if (extension == ".shr")
						res = MemoryLoadSHR(filePath);
					else if (extension == "#C10000")
						res = MemoryLoadSHR(filePath);
					else if (extension == "#C10002")
						res = MemoryLoadSHR(filePath);
					if (res)
					{
						ImGuiFileDialog::Instance()->Close();
						return res;
					}
				}
				// If a file is selected, read and load it into the array
				if (filePath[0] != '\0') {
					res = MemoryLoad(filePath, position, bAuxBank);

					// Reset filePath for next operation
					filePath[0] = '\0';
				}
			}
			ImGuiFileDialog::Instance()->Close();

		}
		return res;
	}

	bool MemoryLoadLGR(const std::string& filePath)
	{
		bool res = false;
		uint8_t* pMem;
		std::ifstream file(filePath, std::ios::binary);
		if (file)
		{
			// Move to the end to get the file size
			file.seekg(0, std::ios::end);
			size_t fileSize = file.tellg();

			if (!((fileSize == 0x400) || (fileSize == 0x800))) {
				std::cerr << "Error: LGR file is not the correct size." << std::endl;
				return res;
			}

			file.seekg(0, std::ios::beg); // Go back to the start of the file
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x400;
			file.read(reinterpret_cast<char*>(pMem), 0x400);
			if (fileSize == 0x800) {	// interlace or page flip
				pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x800;
				file.read(reinterpret_cast<char*>(pMem), 0x400);
			}
			res = true;
		}
		return res;
	}

	bool MemoryLoadDGR(const std::string& filePath)
	{
		bool res = false;
		uint8_t* pMem;
		std::ifstream file(filePath, std::ios::binary);
		if (file)
		{
			// Move to the end to get the file size
			file.seekg(0, std::ios::end);
			size_t fileSize = file.tellg();

			if (!((fileSize == 0x800) || (fileSize == 0x1000))) {
				std::cerr << "Error: DGR file is not the correct size." << std::endl;
				return res;
			}

			file.seekg(0, std::ios::beg); // Go back to the start of the file
										  // Read first the aux and then the main memory
			pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x400;
			file.read(reinterpret_cast<char*>(pMem), 0x400);
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x400;
			file.read(reinterpret_cast<char*>(pMem), 0x400);
			if (fileSize == 0x8000) {	// interlace or page flip
				pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x800;
				file.read(reinterpret_cast<char*>(pMem), 0x400);
				pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x800;
				file.read(reinterpret_cast<char*>(pMem), 0x400);
			}
			res = true;
		}
		return res;
	}

	bool MemoryLoadHGR(const std::string &filePath) {
		bool res = false;
		uint8_t* pMem;
		std::ifstream file(filePath, std::ios::binary);
		if (file)
		{
			// Move to the end to get the file size
			file.seekg(0, std::ios::end);
			size_t fileSize = file.tellg();

			if (!((fileSize == 0x2000) || (fileSize == 0x4000))) {
				std::cerr << "Error: HGR file is not the correct size." << std::endl;
				return res;
			}

			file.seekg(0, std::ios::beg); // Go back to the start of the file
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
			file.read(reinterpret_cast<char*>(pMem), 0x2000);
			if (fileSize == 0x4000) {	// interlace or page flip
				pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x4000;
				file.read(reinterpret_cast<char*>(pMem), 0x2000);
			}
			res = true;
		}
		return res;
	}

	bool MemoryLoadDHR(const std::string &filePath) {
		bool res = false;
		uint8_t* pMem;
		std::ifstream file(filePath, std::ios::binary);
		if (file)
		{
			// Move to the end to get the file size
			file.seekg(0, std::ios::end);
			size_t fileSize = file.tellg();

			if (!((fileSize == 0x4000) || (fileSize == 0x8000))) {
				std::cerr << "Error: DHR file is not the correct size." << std::endl;
				return res;
			}

			file.seekg(0, std::ios::beg); // Go back to the start of the file
										  // Read first the aux and then the main memory
			pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;
			file.read(reinterpret_cast<char*>(pMem), 0x2000);
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
			file.read(reinterpret_cast<char*>(pMem), 0x2000);
			if (fileSize == 0x8000) {	// interlace or page flip
				pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x4000;
				file.read(reinterpret_cast<char*>(pMem), 0x2000);
				pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x4000;
				file.read(reinterpret_cast<char*>(pMem), 0x2000);
			}
			res = true;
		}
		return res;
	}

	bool MemoryLoadSHR(const std::string& filePath) {
		bool res = false;
		auto logManager = LogTextManager::GetInstance();
		std::ifstream file(filePath, std::ios::binary);
		SHRFileContent_e typeE1 = SHRFileContent_e::UNKNOWN;
		SHRFileContent_e typeE0 = SHRFileContent_e::UNKNOWN;
		uint32_t parsedCount = 0;
		if (file)
			parsedCount = ParseSHRData(file, 0, &typeE1, &typeE0);
		if (parsedCount == 0) {
			logManager->AddLog(std::string("Error! SHR unknown file size: " + filePath));
			return res;
		}
		std::string logText = "Loading SHR";
		switch (typeE1)
		{
			case SHRFileContent_e::SHR:
				// standard SHR
				break;
			case SHRFileContent_e::SHR4:
				logText += "4";
				break;
			case SHRFileContent_e::SHR3200:
				logText += "3200";
				break;
			default:
				// this shouldn't happen, no data should be parsed in this case
				logText += " (UNKNOWN!?)";
				break;
		}
		switch (typeE0)
		{
			case SHRFileContent_e::UNKNOWN:
				// Single image, not interlaced or page flipped
				break;
			case SHRFileContent_e::SHR:
				// standard SHR
				logText += " + SHR";
				break;
			case SHRFileContent_e::SHR4:
				logText += " + SHR4";
				break;
			case SHRFileContent_e::SHR3200:
				logText += " + SHR3200";
				break;
			case SHRFileContent_e::SHR_BYTES:
				logText += " + SHR interlace bytes (no SCB+palettes)";
				break;
			default:
				break;
		}

		// Move again to the end to get the file size
		file.seekg(0, std::ios::end);
		size_t fileSize = file.tellg();
		auto _remainingSize = fileSize - parsedCount;
		if (_remainingSize != 0)
			logText += " + " + std::to_string(_remainingSize) + " unknown bytes";

		logText += ": " + filePath;
		logManager->AddLog(logText);
		return true;
	}

	uint32_t ParseSHRData(std::ifstream& file, uint32_t offset, SHRFileContent_e* typeE1, SHRFileContent_e* typeE0) {
		uint8_t* pMem;
		uint32_t magicBytes;
		if (file)
		{
			auto memManager = MemoryManager::GetInstance();
			// Move to the end to get the file size
			file.seekg(0, std::ios::end);
			size_t fileSize = file.tellg();
			*typeE1 = SHRFileContent_e::UNKNOWN;
			*typeE0 = SHRFileContent_e::UNKNOWN;

			if (fileSize < 0x8000) {
				return 0;
			}

			file.seekg(offset); // Go back to the offset requested
			pMem = memManager->GetApple2MemAuxPtr();
			*typeE1 = SHRFileContent_e::SHR;
			file.read(reinterpret_cast<char*>(pMem + 0x2000), 0x8000);	// standard SHR structure always in the first 0x8000
			magicBytes = reinterpret_cast<uint32_t*>(pMem + _A2VIDEO_SHR_MAGIC_BYTES)[0];
			if (magicBytes == _A2VIDEO_SHR4_MAGIC_STRING) {
				// the first image is actually an SHR4
				*typeE1 = SHRFileContent_e::SHR4;
			}
			if (fileSize == 0x10000) {	// interlace or page flip, read the next SHR
				pMem = MemoryManager::GetInstance()->GetApple2MemPtr();
				*typeE0 = SHRFileContent_e::SHR;
				file.read(reinterpret_cast<char*>(pMem + 0x2000), 0x8000);
				magicBytes = reinterpret_cast<uint32_t*>(pMem + _A2VIDEO_SHR_MAGIC_BYTES)[0];
				if (magicBytes == _A2VIDEO_SHR4_MAGIC_STRING) {
					// the second image is actually an SHR4
					*typeE0 = SHRFileContent_e::SHR4;
				}
			}
			else if (fileSize >= 0x9900) {	// SHR3200 or more
				if (magicBytes == _A2VIDEO_3200_MAGIC_STRING) {
					// the first image is a SHR3200
					*typeE1 = SHRFileContent_e::SHR3200;
					// where should the palettes be loaded? (See A2VideoManager.cpp)
					uint8_t* pPalStart = memManager->GetApple2MemAuxPtr() + _A2VIDEO_SHR_MAGIC_BYTES - 4;
					uint8_t* bankPtr = (pPalStart[1] == 1 ? memManager->GetApple2MemAuxPtr() : memManager->GetApple2MemPtr());
					uint16_t palStart = (((uint16_t)pPalStart[3]) << 8) | pPalStart[2];
					file.read(reinterpret_cast<char*>(bankPtr + palStart), _A2VIDEO_SHR_SCANLINES * 32); // 0x1900 palette bytes
				}
				// now check what else is in there. Could be another SHR 3200 or whatever
				auto _remainingSize = fileSize - 0x9900;
				if (_remainingSize > 0) {
					pMem = MemoryManager::GetInstance()->GetApple2MemPtr();	// 2nd image always in E0/Main
					if (_remainingSize == 0x8000) {		// there's an SHR in there
						*typeE0 = SHRFileContent_e::SHR;
						file.read(reinterpret_cast<char*>(pMem + 0x2000), 0x8000);
						magicBytes = reinterpret_cast<uint32_t*>(pMem + _A2VIDEO_SHR_MAGIC_BYTES)[0];
						if (magicBytes == _A2VIDEO_SHR4_MAGIC_STRING) {
							// the second image is actually an SHR4
							*typeE0 = SHRFileContent_e::SHR4;
						}
					}
					else if (_remainingSize == 0x9900) {	// Another SHR3200 in there
						*typeE0 = SHRFileContent_e::SHR3200;
						file.read(reinterpret_cast<char*>(pMem + 0x2000), 0x8000);
						uint8_t* pPalStart = pMem + _A2VIDEO_SHR_MAGIC_BYTES - 4;
						uint8_t* bankPtr = (pPalStart[1] == 1 ? memManager->GetApple2MemAuxPtr() : memManager->GetApple2MemPtr());
						uint16_t palStart = (((uint16_t)pPalStart[3]) << 8) | pPalStart[2];
						file.read(reinterpret_cast<char*>(bankPtr + palStart), _A2VIDEO_SHR_SCANLINES * 32); // 0x1900 palette bytes
					}
					else if (_remainingSize == 0x7D00) {
						// Just 32000 SHR line bytes, which we'll use as interlace/flip data
						// and we'll copy over the palettes, scb and other stuff
						*typeE0 = SHRFileContent_e::SHR_BYTES;
						file.read(reinterpret_cast<char*>(pMem), 0x7D00);
						std::memcpy(pMem + 0x7D00, memManager->GetApple2MemAuxPtr() + 0x7D00, 0x8000-0x7D00);
					}
					else {
						// there are _remainingSize bytes, do nothing with them
					}
				}
			}
		}
		return (uint32_t)file.tellg();
	}

	std::string GetMemorySaveFilePath() {
		std::filesystem::path appRoot = std::filesystem::current_path();
		std::filesystem::path screenshotsDir = appRoot / "screenshots";
		if (!std::filesystem::exists(screenshotsDir)) {
			if (!std::filesystem::create_directory(screenshotsDir)) {
				throw std::runtime_error("Failed to create directory: " + screenshotsDir.string());
			}
		}

		auto now = std::chrono::system_clock::now();
		std::time_t now_c = std::chrono::system_clock::to_time_t(now);
		std::tm tm_local;
#ifdef _WIN32
		localtime_s(&tm_local, &now_c);  // Thread-safe on Windows
#else
		std::tm* tm_ptr = std::localtime(&now_c);  // Note: not thread-safe
		if (tm_ptr) {
			tm_local = *tm_ptr;
		}
		else {
			throw std::runtime_error("Failed to convert current time.");
		}
#endif
		std::stringstream ss;
		ss << std::put_time(&tm_local, "%Y%m%d_%H%M%S");	// "YYYYMMDD_HHMMSS"
		std::string timestamp = ss.str();

		// Construct the file path: screenshots/<timestamp>
		std::filesystem::path filePath = screenshotsDir / timestamp;

		return filePath.string();
	}

	bool MemorySaveLGR(const std::string& filePath, size_t fileSize /*= 0x400*/)
	{
		uint8_t* pMem;
		if (!((fileSize == 0x400) || (fileSize == 0x800))) {
			std::cerr << "Error: LGR file is not the correct size." << std::endl;
			return false;
		}

		std::string fullPath = filePath + ".lgr";
		std::ofstream outFile(fullPath, std::ios::binary);
		if (!outFile) {
			std::cerr << "Error: Failed to open file for writing: " << fullPath << std::endl;
			return false;
		}
		pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x400;
		outFile.write(reinterpret_cast<char*>(pMem), 0x400);
		if (!outFile) {
			std::cerr << "Error: Failed to write main memory data to file: " << fullPath << std::endl;
			return false;
		}
		if (fileSize == 0x800) {
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x800;
			outFile.write(reinterpret_cast<char*>(pMem), 0x400);
			if (!outFile) {
				std::cerr << "Error: Failed to write main page2 memory data to file: " << fullPath << std::endl;
				return false;
			}
		}
		outFile.close();
		return true;
	}

	bool MemorySaveDGR(const std::string& filePath, size_t fileSize /*= 0x800*/)
	{
		uint8_t* pMem;
		if (!((fileSize == 0x800) || (fileSize == 0x1000))) {
			std::cerr << "Error: DHR file is not the correct size." << std::endl;
			return false;
		}

		std::string fullPath = filePath + ".dgr";
		std::ofstream outFile(fullPath, std::ios::binary);
		if (!outFile) {
			std::cerr << "Error: Failed to open file for writing: " << fullPath << std::endl;
			return false;
		}
		pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x400;
		outFile.write(reinterpret_cast<char*>(pMem), 0x400);
		if (!outFile) {
			std::cerr << "Error: Failed to write auxiliary memory data to file: " << fullPath << std::endl;
			return false;
		}
		pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x400;
		outFile.write(reinterpret_cast<char*>(pMem), 0x400);
		if (!outFile) {
			std::cerr << "Error: Failed to write main memory data to file: " << fullPath << std::endl;
			return false;
		}
		if (fileSize == 0x1000) {
			pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x800;
			outFile.write(reinterpret_cast<char*>(pMem), 0x400);
			if (!outFile) {
				std::cerr << "Error: Failed to write auxiliary page2 memory data to file: " << fullPath << std::endl;
				return false;
			}
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x800;
			outFile.write(reinterpret_cast<char*>(pMem), 0x400);
			if (!outFile) {
				std::cerr << "Error: Failed to write main page2 memory data to file: " << fullPath << std::endl;
				return false;
			}
		}
		outFile.close();
		return true;
	}

	bool MemorySaveHGR(const std::string& filePath, size_t fileSize /*= 0x2000*/)
	{
		uint8_t* pMem;
		if (!((fileSize == 0x2000) || (fileSize == 0x4000))) {
			std::cerr << "Error: HGR file is not the correct size." << std::endl;
			return false;
		}

		std::string fullPath = filePath + ".hgr";
		std::ofstream outFile(fullPath, std::ios::binary);
		if (!outFile) {
			std::cerr << "Error: Failed to open file for writing: " << fullPath << std::endl;
			return false;
		}
		pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
		outFile.write(reinterpret_cast<char*>(pMem), 0x2000);
		if (!outFile) {
			std::cerr << "Error: Failed to write main memory data to file: " << fullPath << std::endl;
			return false;
		}
		if (fileSize == 0x4000) {
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x4000;
			outFile.write(reinterpret_cast<char*>(pMem), 0x2000);
			if (!outFile) {
				std::cerr << "Error: Failed to write main page2 memory data to file: " << fullPath << std::endl;
				return false;
			}
		}
		outFile.close();
		return true;
	}

	bool MemorySaveDHR(const std::string& filePath, size_t fileSize /*= 0x4000*/)
	{
		uint8_t* pMem;
		if (!((fileSize == 0x4000) || (fileSize == 0x8000))) {
			std::cerr << "Error: DHR file is not the correct size." << std::endl;
			return false;
		}

		std::string fullPath = filePath + ".dhr";
		std::ofstream outFile(fullPath, std::ios::binary);
		if (!outFile) {
			std::cerr << "Error: Failed to open file for writing: " << fullPath << std::endl;
			return false;
		}
		pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;
		outFile.write(reinterpret_cast<char*>(pMem), 0x2000);
		if (!outFile) {
			std::cerr << "Error: Failed to write auxiliary memory data to file: " << fullPath << std::endl;
			return false;
		}
		pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
		outFile.write(reinterpret_cast<char*>(pMem), 0x2000);
		if (!outFile) {
			std::cerr << "Error: Failed to write main memory data to file: " << fullPath << std::endl;
			return false;
		}
		if (fileSize == 0x4000) {
			pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x4000;
			outFile.write(reinterpret_cast<char*>(pMem), 0x2000);
			if (!outFile) {
				std::cerr << "Error: Failed to write auxiliary page2 memory data to file: " << fullPath << std::endl;
				return false;
			}
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x4000;
			outFile.write(reinterpret_cast<char*>(pMem), 0x2000);
			if (!outFile) {
				std::cerr << "Error: Failed to write main page2 memory data to file: " << fullPath << std::endl;
				return false;
			}
		}
		outFile.close();
		return true;
	}

	bool MemorySaveSHR(const std::string& filePath, size_t fileSize /*= 0x80000*/)
	{
		uint8_t* pMem;
		pMem = MemoryManager::GetInstance()->GetApple2MemAuxPtr() + 0x2000;

		if (!((fileSize == 0x8000) || (fileSize == 0x10000))) {
			std::cerr << "Error: SHR file is not the correct size." << std::endl;
			return false;
		}

		std::string fullPath = filePath + ".shr";
		std::ofstream outFile(fullPath, std::ios::binary);
		if (!outFile) {
			std::cerr << "Error: Failed to open file for writing: " << fullPath << std::endl;
			return false;
		}
		outFile.write(reinterpret_cast<char*>(pMem), 0x8000);
		if (!outFile) {
			std::cerr << "Error: Failed to write E1 memory data to file: " << fullPath << std::endl;
			return false;
		}
		if (fileSize == 0x10000) {
			pMem = MemoryManager::GetInstance()->GetApple2MemPtr() + 0x2000;
			outFile.write(reinterpret_cast<char*>(pMem), 0x8000);
			if (!outFile) {
				std::cerr << "Error: Failed to write E0 memory data to file: " << fullPath << std::endl;
				return false;
			}
		}
		outFile.close();
		return true;
	}

} // namespace sdd
