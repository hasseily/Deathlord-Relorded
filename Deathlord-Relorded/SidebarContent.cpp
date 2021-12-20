#include "pch.h"
#include "SidebarContent.h"
#include "Emulator/Memory.h"
#include <shobjidl_core.h> 
#include <DirectXPackedVector.h>
#include <DirectXMath.h>
#include <Sidebar.h>
#include "HAUtils.h"
#include "Game.h"   // for g_isInGameMap and ticks

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace DirectX::PackedVector;
using namespace std;
using namespace nlohmann;
namespace fs = std::filesystem;

static string SIDEBAR_FORMAT_PLACEHOLDER("{}");

static UINT8* pmem;
static int memsize;

static char deathlordCharMap[106] = "edgfa`cbmlonihkjutwvqpsr  ][yx*zEDGFA@CBMLONIHKJUTWVQPSR    YX-Z%$'&! #\" - , / .)(+*54761032 = < ? >98; :";
static char tempBuf[500] = "";

SidebarContent::SidebarContent()
{
    Initialize();
}

void SidebarContent::Initialize()
{
    // Get memory start address
    memsize = 0x20000;  // 128k of RAM
    nextSidebarToRender = 0;
	nextBlockToRender = 0;
}

bool SidebarContent::setActiveProfile(SidebarManager* sbM, std::string* name)
{
    if (name->empty())
    {
        // this happens if we can't read the shm
        // TODO: load a default profile that says that AW isn't running?
        // Or put other default info?
        return false;
    }
    /*
    if ((!force) && (m_activeProfile["meta"]["name"] == *name))
    {
        // no change
        return true;
    }
    */
	nlohmann::json j;
    try {
        j = m_allProfiles.at(*name);
    }
    catch (std::out_of_range const& exc) {
        m_activeProfile["meta"]["name"] = *name;
        SidebarExceptionHandler(exc.what());
        //char buf[sizeof(exc.what()) + 500];
        //snprintf(buf, 500, "Profile %s doesn't exist: %s\n", name->c_str(), exc.what());
        //OutputDebugStringA(buf);
        return false;
    }

    m_activeProfile = j;
    //OutputDebugStringA(j.dump().c_str());
    //OutputDebugStringA(j["sidebars"].dump().c_str());

    sbM->DeleteAllSidebars();
    string title;
    auto numSidebars = (UINT8)m_activeProfile["sidebars"].size();
    for (UINT8 i = 0; i < numSidebars; i++)
    {
        nlohmann::json sj = m_activeProfile["sidebars"][i];
        auto numBlocks = static_cast<UINT8>(sj["blocks"].size());
        if (numBlocks == 0)
        {
            continue;
        }

        SidebarTypes st = SidebarTypes::Right;  // default
        UINT16 sSize = 0;
        if (sj["type"] == "Bottom")
        {
            st = SidebarTypes::Bottom;
        }
        switch (st)
        {
        case SidebarTypes::Right:
            if (sj.contains("width"))
                sSize = sj["width"];
            break;
        case SidebarTypes::Bottom:
            if (sj.contains("height"))
                sSize = sj["height"];
            break;
        default:
            break;
        }
        UINT8 sbId;
        sbM->CreateSidebar(st, numBlocks, sSize, &sbId);
        for (UINT8 k = 0; k < numBlocks; k++)
        {
            nlohmann::json bj = sj["blocks"][k];
            // OutputDebugStringA((bj.dump() + string("\n")).c_str());
            BlockStruct bS;

            if (bj["type"] == "Header")
            {
                bS.color = Colors::CadetBlue;
                bS.type = BlockType::Header;
                bS.fontId = FontDescriptors::FontA2Bold;
            }
            else if (bj["type"] == "Empty")
            {
                bS.color = Colors::Black;
                bS.type = BlockType::Empty;
                bS.fontId = FontDescriptors::FontA2Regular;
            }
            else if (bj["type"] == "Italic")
			{
				bS.color = Colors::GhostWhite;
				bS.type = BlockType::Content;
				bS.fontId = FontDescriptors::FontA2Italic;
			}
			else if (bj["type"] == "BoldItalic")
			{
				bS.color = Colors::GhostWhite;
				bS.type = BlockType::Content;
				bS.fontId = FontDescriptors::FontA2BoldItalic;
			}
		    else // default to "Content"
            {
                bS.color = Colors::GhostWhite;
                bS.type = BlockType::Content;
                bS.fontId = FontDescriptors::FontA2Regular;
            }
            // overrides
            if (bj.contains("color"))
            {
                bS.color = XMVectorSet(bj["color"][0], bj["color"][1], bj["color"][2], bj["color"][3]);
            }
            bS.text = "";
            sbM->sidebars[sbId].SetBlock(bS, k);
        }
    }
    // Now force an update of the sidebar text once, so all the static data is updated
	UpdateAllSidebarText(sbM, true);
    return true;
}

void SidebarContent::LoadProfileUsingDialog(SidebarManager* sbM)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        IFileOpenDialog* pFileOpen;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr))
        {
			COMDLG_FILTERSPEC rgSpec[] =
			{
				{ L"JSON Profiles", L"*.json" },
                { L"All Files", L"*.*" },
			};
            pFileOpen->SetFileTypes(2, rgSpec);
            // Show the Open dialog box.
            hr = pFileOpen->Show(nullptr);
            // Get the file name from the dialog box.
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    // Display the file name to the user.
                    if (SUCCEEDED(hr))
                    {
                        fs::directory_entry dir = fs::directory_entry(pszFilePath);
                        std::string profileName = OpenProfile(dir);
                        if (setActiveProfile(sbM, &profileName))
                        {
                            g_nonVolatile.profilePath = std::wstring(pszFilePath);
                        }
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
}

void SidebarContent::LoadProfilesFromDisk()
{
    if(!m_allProfiles.empty())
        m_allProfiles.clear();
    fs::path currentDir = fs::current_path();
    currentDir += "\\Profiles";
    
    for (const auto& entry : fs::directory_iterator(currentDir))
    {
        OpenProfile(entry);
    }
}

std::string SidebarContent::OpenProfile(std::wstring entry)
{
    if (entry.size() == 0)
        return "";
    try
   {
        fs::directory_entry thePath = fs::directory_entry(entry);
        HA::ConvertStrToWStr(&thePath.path().string(), &g_nonVolatile.profilePath);
        g_nonVolatile.SaveToDisk();
	    return SidebarContent::OpenProfile(thePath);
   }
   catch (...)
   {
       return "";
   }
}

std::string SidebarContent::OpenProfile(std::filesystem::directory_entry entry)
{
    Initialize();   // Might have lost the shm
//    if (entry.is_regular_file() && (entry.path().extension().compare("json")))
    if (entry.is_regular_file())
    {
        nlohmann::json profile = ParseProfile(entry.path());
        if (profile != nullptr)
        {
            string name = profile["meta"]["name"];
            if (!name.empty())
            {
                m_allProfiles[name] = profile;
                return name;
            }
        }
    }
    return "";
}

void SidebarContent::ClearActiveProfile(SidebarManager* sbM)
{
    m_activeProfile.clear();
    sbM->DeleteAllSidebars();
}

nlohmann::json SidebarContent::ParseProfile(fs::path filepath)
{
    try
    {
        std::ifstream i(filepath);
        nlohmann::json j;
        i >> j;
        return j;
    }
    catch (nlohmann::detail::parse_error err) {
        char buf[sizeof(err.what()) + 500];
        snprintf(buf, 500, "Error parsing profile: %s\n", err.what());
		SidebarExceptionHandler(buf);
        //OutputDebugStringA(buf);
        return nullptr;
    }
}

// Turn a variable into a string
// using the following json format:
/*
    {
        "memstart": "0x1165A",
        "length" : 16,
        "type" : "ascii_high"
    }
*/

std::string SidebarContent::SerializeVariable(nlohmann::json* pvar)
{
	string s;
    ZeroMemory(tempBuf, sizeof(tempBuf));
    if (!g_isInGameMap)
        return s;
    nlohmann::json j = *pvar;
    // initialize variables
    // OutputDebugStringA((j.dump()+string("\n")).c_str());
    int memOffset;
	if (j.count("mem") != 1)
		return s;
	if (j.count("type") != 1)
		return s;
    auto numMemLocations = (UINT8)j["mem"].size();
    if (numMemLocations == 0)
        return s;

	// since the memory is contiguous, no need to check if the info is in main or aux mem
    // and since we're only reading, we don't care if the mem is dirty
	pmem = MemGetRealMainPtr(0);
	if (pmem == nullptr)
		return s;

    // Start with string and lookups, which don't use more than one mem location

	// In Deathlord, a string has variable length and ends with the char having its high bit set
	if (j["type"] == "string")
	{
		size_t i = 0;
		UINT8 c;
        memOffset = std::stoul(j["mem"][0].get<std::string>(), nullptr, 0);
		while (*(pmem + memOffset + i) < 0x80)
		{
			c = *(pmem + memOffset + i);
			if (c < sizeof(deathlordCharMap))
				s.append(1, deathlordCharMap[c]);
			++i;
            if (i > 1000) // don't go ballistic!
                break;
		}
		// Last char with high bit set
        c = *(pmem + memOffset + i) - 0x80;
		s.append(1, deathlordCharMap[c]);
        return s;
	}

    // Similarly, lookups don't use more than one mem location (a lookup of more than 255 items is not supported)
	if (j["type"] == "lookup")
	{
		memOffset = std::stoul(j["mem"][0].get<std::string>(), nullptr, 0);
		try
		{
			snprintf(tempBuf, sizeof(tempBuf), "%s/0x%02x", j["lookup"].get<std::string>().c_str(), *(pmem + memOffset));
			nlohmann::json::json_pointer jp(tempBuf);
			return m_activeProfile.value(jp, "-");  // Default value when unknown lookup is "-"
		}
		catch (exception e)
		{
			std::string es = j.dump().substr(0, 400);
			char buf[sizeof(e.what()) + 500];
			sprintf_s(buf, "Error parsing lookup: %s\n%s\n", es.c_str(), e.what());
			SidebarExceptionHandler(buf);
			//OutputDebugStringA(buf);
            return s;
		}
	}

	// Lookup Aggregate is a summation of a lookup on all the requested mem areas
    // So you loop through the mem areas, do a lookup on a table whose keys are mem areas and values are numbers
    // and you add them up
	if (j["type"] == "lookup_aggregate")
	{
		try
		{
			int aggregate = 0;
            if (j.count("startvalue") == 1)
                aggregate = j["startvalue"].get<int>();
			for (size_t memIdx = 0; memIdx < numMemLocations; memIdx++)
			{
                memOffset = std::stoul(j["mem"][memIdx].get<std::string>(), nullptr, 0);
                UINT8 theMem = *(pmem + memOffset);
				snprintf(tempBuf, sizeof(tempBuf), "%s/0x%02x", j["lookup"].get<std::string>().c_str(), theMem);
                nlohmann::json::json_pointer jp(tempBuf);
                aggregate += m_activeProfile.value<int>(jp, 0); // Default value when unknown lookup is 0
			}
			s = to_string(aggregate);
			return s;
		}
		catch (exception e)
		{
			std::string es = j.dump().substr(0, 400);
			char buf[sizeof(e.what()) + 500];
			sprintf_s(buf, "Error parsing lookup: %s\n%s\n", es.c_str(), e.what());
			SidebarExceptionHandler(buf);
			//OutputDebugStringA(buf);
			return s;
		}
	}

    // Now loop through each memory location in case of integers
    // Starting with the lowest byte
    if (j["type"] == "int")
    {
		int x = 0;
        for (size_t memIdx = 0; memIdx < numMemLocations; memIdx++)
        {
            memOffset = std::stoul(j["mem"][memIdx].get<std::string>(), nullptr, 0);
            x += (*(pmem + memOffset)) * (int)pow(0x100, memIdx);
        }
		char cbuf[100];
		snprintf(cbuf, 100, "%d", x);
		s.insert(0, string(cbuf));
        return s;
    }

	// int literal is like what is used in the Ultima games.
    // Garriott stored ints as literals inside memory, so for example
    // a hex 0x4523 is in fact the number 4523
    if (j["type"] == "int_literal")
    {
        for (size_t memIdx = 0; memIdx < numMemLocations; memIdx++)
        {
            memOffset = std::stoul(j["mem"][memIdx].get<std::string>(), nullptr, 0);

            char cbuf[100];
            snprintf(cbuf, 100, "%x", *(pmem + memOffset));
            s.insert(0, string(cbuf));
        }
        return s;
    }

	char buf[500];
	sprintf_s(buf, "Unknown Type");
	SidebarExceptionHandler(buf);
    return s;
}

// This method formats the whole text block using the format string and vars
// Kind of like sprintf using the following json format:
/*
{
    "type": "Content",
    "template" : "Left: {} - Right: {}",
    "vars" : [
                {
                    "mem": ["0x1165A"],
                    "type" : "string"
                },
                {
                    "mem": ["0x1165E"],
                    "type" : "string"
                }
            ]
}
*/

std::string SidebarContent::FormatBlockText(nlohmann::json* pdata)
{
// serialize the SHM variables into strings
// and directly put them in the format string
    nlohmann::json data = *pdata;
    string txt;
    if (data.contains("template"))
    {
        txt = data["template"].get<string>();
    }
    array<string, SIDEBAR_MAX_VARS_IN_BLOCK> sVars;
    for (size_t i = 0; i < data["vars"].size(); i++)
    {
        sVars[i] = SerializeVariable(&(data["vars"][i]));
        if (data["vars"][i].contains("fixedlength"))
        {
            char padchar = ' ';
            if (data["vars"][i].contains("padchar"))
            {
                string sPadChar = data["vars"][i]["padchar"];
                padchar = sPadChar.c_str()[0];
            }
            // prepend for integers
            if ((data["vars"][i]["type"] == "int") || (data["vars"][i]["type"] == "int_literal"))
            {
                int numCharsToInsert = (int)data["vars"][i]["fixedlength"] - sVars[i].size();
                sVars[i].insert(0, numCharsToInsert, padchar);
            }
            else
            {
                // append for strings
                sVars[i].resize((int)data["vars"][i]["fixedlength"], padchar);
            }
        }
        txt.replace(txt.find(SIDEBAR_FORMAT_PLACEHOLDER), SIDEBAR_FORMAT_PLACEHOLDER.length(), sVars[i]);
    }
    return txt;
}

UINT64 SidebarContent::UpdateAllSidebarText(SidebarManager* sbM, bool forceUpdate, UINT64 maxMicroSecondsAllowed)
{
    // Initialize timer info
    LARGE_INTEGER startTicks, currTicks, elapsedMicroSeconds;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&startTicks);
    QueryPerformanceFrequency(&frequency);
    elapsedMicroSeconds.QuadPart = 0;

    // Initialize block and sidebar info
	auto sbArray = m_activeProfile["sidebars"];
	json::iterator sbIt = sbArray.begin();
	int sbCt = sbArray.size();

    // If we're asked to update everything, start from the beginning and go all the way through all sidebars
    if (forceUpdate)
    {
		nextBlockToRender = 0;
		nextSidebarToRender = 0;
    }

BEGIN:
	// Ensure that the next sidebar and block to render are valid
    // If the block to render is over the limit, go either to the next sidebar
    // or if there is none left, go back to the start
    // 
    // If finished all sidebars, reset to beginning and exit
    // Let the next call to this method start over
    if (nextSidebarToRender >= sbCt)
    {
        nextBlockToRender = 0;
        nextSidebarToRender = 0;
        return elapsedMicroSeconds.QuadPart;
    }
    // if finished a sidebar, go to the next one
    if (nextBlockToRender >= m_activeProfile["sidebars"].at(nextSidebarToRender)["blocks"].size())
    {
        nextBlockToRender = 0;
        ++nextSidebarToRender;
        goto BEGIN;
    }

    // Process blocks, starting at nextBlockToRender,
    // keeping track of where we stopped
	sbIt = sbArray.begin();
    sbIt += nextSidebarToRender;
    auto bArray = (*sbIt)["blocks"];
    json::iterator bIt = bArray.begin();
    bIt += nextBlockToRender;

    auto bl = *bIt;
    if (!forceUpdate)
    {
        // don't update any static block unless we force update
        if ((bl.count("vars") == 0) || (bl["vars"].size() == 0))
        {
            ++nextBlockToRender;
            goto BEGIN;
        }
    }
    if (!UpdateBlock(sbM, nextSidebarToRender, nextBlockToRender, &bl))
    {
#ifdef _DEBUG
        OutputDebugStringA((bl.dump() + string("\n")).c_str());
#endif // _DEBUG
    }
    ++nextBlockToRender;

    // check if we've gone above our allotted time (if unforced update). If so, exit while remembering where we were
    QueryPerformanceCounter(&currTicks);
    elapsedMicroSeconds.QuadPart = currTicks.QuadPart - startTicks.QuadPart;
    elapsedMicroSeconds.QuadPart *= 1000000;
    elapsedMicroSeconds.QuadPart /= frequency.QuadPart;
    if (forceUpdate)
        goto BEGIN;
    if ((elapsedMicroSeconds.QuadPart) < maxMicroSecondsAllowed)
        goto BEGIN;

    return elapsedMicroSeconds.QuadPart;
}


// Update and send for display a block of text
// using the following json format:
/*
{
    "type": "Content",
    "template" : "Left: {} - Right: {}",
    "vars" : [
				{
					"mem": ["0x1165A"],
					"type" : "string"
				},
				{
					"mem": ["0x1165E"],
					"type" : "string"
				}
            ]
}
*/

bool SidebarContent::UpdateBlock(SidebarManager* sbM, UINT8 sidebarId, UINT8 blockId, nlohmann::json* pdata)
{
    nlohmann::json data = *pdata;
    // OutputDebugStringA((data.dump()+string("\n")).c_str());

    try
    {
        Sidebar sb = sbM->sidebars.at(sidebarId);
        auto b = sb.blocks.at(blockId);
        string s;
        switch (b->type)
        {
        case BlockType::Empty:
            return true;
            break;
        case BlockType::Header:
            s = FormatBlockText(pdata);
            break;
        default:                    // Content
            s = FormatBlockText(pdata);
            break;
        }

        //OutputDebugStringA(s.c_str());
        //OutputDebugStringA("\n");
        if (sb.SetBlockText(s, blockId) == SidebarError::ERR_NONE)
        {
            return true;
        }
        return false;
    }
    catch (...)
    {
        return false;
    }
}
