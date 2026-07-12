// Extras functions to load a binary file into an arbitrary location in memory
// Also functions to save a binary file into a location on the filesystem

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>
#include <string>

namespace sdd {
	
	enum class SHRFileContent_e
	{
		UNKNOWN = 0,			// Unknown, can't parse
		SHR,					// 0x8000: Standard SHR
		SHR4,					// 0x8000: SHR4
		SHR3200,				// 0x9900: SHR3200 (Brooks) with 200 palettes
		SHR_BYTES,				// 0x7D00: Raw SHR bytes (no SCB or palettes)
		TOTAL_COUNT
	};

	bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank, std::string& path);
	bool MemoryLoad(const std::string &filePath, uint32_t position, bool bAuxBank, size_t fileSize = 0);
	bool MemoryLoadLGR(const std::string &filePath);
	bool MemoryLoadDGR(const std::string &filePath);
	bool MemoryLoadHGR(const std::string &filePath);
	bool MemoryLoadDHR(const std::string &filePath);
	bool MemoryLoadSHR(const std::string &filePath);
	// Loads an SHR file starting at offset. Returns if parsed bytes, and the content loaded in E1 (aux) and E0 (main)
	uint32_t ParseSHRData(std::ifstream& file, uint32_t offset, SHRFileContent_e* typeE1, SHRFileContent_e* typeE0);
	// Returns a fully formatted file path for saving, no suffix
	std::string GetMemorySaveFilePath();
	// the save methods auto-append the correct suffix (.hgr, .dgr, .shr)
	bool MemorySaveLGR(const std::string& filePath, size_t fileSize = 0x400);	// 0x800 for interlace/pageflip
	bool MemorySaveDGR(const std::string& filePath, size_t fileSize = 0x800);	// 0x1000 for interlace/pageflip
	bool MemorySaveHGR(const std::string& filePath, size_t fileSize = 0x2000);	// 0x4000 for interlace/pageflip
	bool MemorySaveDHR(const std::string& filePath, size_t fileSize = 0x4000);	// 0x8000 for interlace/pageflip
	bool MemorySaveSHR(const std::string& filePath, size_t fileSize = 0x80000);	// 0x10000 for interlace/pageflip

} // namespace sdd

#endif /* MEMORYLOADER_H */
