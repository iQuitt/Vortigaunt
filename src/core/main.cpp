#ifdef _WIN32
#include <windows.h>
#endif
#include <filesystem>


#ifdef ENABLE_LITHTECH
#include "de_file.h"
#include "model.h"
#include "LtbConverter.h"
#include "DtxConverter.h"
#include "core/extractors/rez/RezExtractor.h"
#endif

#include "core/extractors/xfs/XfsExtractor.h"
#include "core/extractors/pak/PakFile.h"
#include "core/extractors/unity/UnityPorter.h"
#include "util.hpp"
#include "fsutils.hpp"

#ifdef ENABLE_GRANNY2
#include "core/converters/Gr2Converter.h"
#endif

#include "AudioConvert.h"
#include "WadMaker.h"

typedef std::vector<std::string> FilePathVec;

#ifdef ENABLE_LITHTECH
auto g_ltbConverter = std::make_unique<ltbConverter>();
auto g_dtxConverter = std::make_unique<DtxConverter>();
auto g_rezExtractor = std::make_unique<RezExtractor>();
//ltbConverter* g_ltbConverter = new ltbConverter();
//DtxConverter* g_dtxConverter = new DtxConverter();
//RezExtractor* g_rezExtractor = new RezExtractor();
#endif

auto g_xfsExtractor = std::make_unique<XfsExtractor>();
//XfsExtractor* g_xfsExtractor = new XfsExtractor();


#ifdef ENABLE_GRANNY2
auto g_gr2Converter = std::make_unique<Gr2Converter>();
//Gr2Converter* g_gr2Converter = new Gr2Converter();
#endif

auto g_audioConverter = std::make_unique<AudioConverter>();
auto g_wadArchive = std::make_unique<WadArchive>();
//AudioConverter* g_audioConverter = new AudioConverter();
//WadArchive* g_wadArchive = new WadArchive();

int findLastOf(const std::string& str,char _Ch) 
{
	int ret = -1;
	int charIndex = -1;
	for (auto& c : str)
	{
		++charIndex;
		if (c == _Ch)
		{
			ret = charIndex;
		}
	}
	return ret;
}

std::string grabFileExt(std::string filePath) 
{
	int dotIndex = findLastOf(filePath, '.');
	if (dotIndex == -1) 
	{
		return std::string("");
	}
	std::string ext = filePath.substr(dotIndex + 1, filePath.length() - dotIndex);
	for (auto& c : ext) 
		c = tolower(c);
	return ext;
}

std::string replaceFileExt(std::string filePath, std::string ext)
{
	int dotIndex = findLastOf(filePath, '.');
	std::string pathWithoutExt = filePath.substr(0, dotIndex);
	return pathWithoutExt + '.' + ext;
}

#ifdef ENABLE_LITHTECH
void ConvertDTX(const std::string& inputFilePath,const std::string& inputFormat,int argc, char** argv)
{
	std::string outFile;
	std::string outFormat;

	if (argc > 2)
	{
		outFile = argv[2];
		outFormat = grabFileExt(outFile);
	}
	else
	{
		outFile = replaceFileExt(inputFilePath, "bmp");
		outFormat = "bmp";
	}
	printf("Converting .DTX : %s -->> %s\n", inputFilePath.c_str(), outFile.c_str());
	if (outFormat != "bmp") 
	{
		printf("Error: Output format must be .bmp!\n");
		return;
	}
	int ret = g_dtxConverter->ConvertSingleDTXFile(outFormat, inputFilePath, outFile);
	if (ret == 0) 
	{
		printf("DTX conversion successful!\n");
	}
	else 
	{
		printf("DTX conversion failed!\n");
	}
}

void ConvertLTB(const std::string& inputFilePath, const std::string& inputFormat, int argc, char** argv)
{
	std::string outFile;
	std::string outFormat;
	if (argc > 2)
	{
		ltbConverterSetting setting;
		for (int i = 2; i < argc; ++i)
		{
			std::string option = argv[i];
			if (option.compare("-ignoreMeshes") == 0)
			{
				setting.IgnoreMeshes = true;
			}
			else if (option.compare("-ignoreAnimations") == 0)
			{
				setting.IgnoreAnimations = true;
			}
			else 
			{
			// If not an option, treat as output file path
				outFile = argv[i];
				outFormat = grabFileExt(outFile);
			}
		}
		g_ltbConverter->SetConvertSetting(setting);
	}
	printf("Converting .LTB : %s -->> %s\n", inputFilePath.c_str(), outFile.c_str());
	if (inputFormat.compare("ltb") != 0)
	{
		printf("Error: Input file format must be .ltb!\n");
		return;
	}
	if (outFormat.compare("ltb") == 0)
	{
		printf("Error: Cannot export to .ltb format!\n");
		return;
	}
	if (outFormat.compare("smd") == 0)
	{
		printf("Exporting SMD format (Valve GoldSource) - Mesh and animations will be exported separately.\n");
	}
	//
	int ret = g_ltbConverter->ConvertSingleLTBFile(inputFilePath, outFile);
	if (ret != CONVERT_RET_OK)
	{
		if (ret == CONVERT_RET_INVALID_INPUT_FILE)
		{
			printf("Error: Failed to open input file!\n");
		}
		else if (ret == CONVERT_RET_LOADING_MODEL_FAILED)
		{
			printf("Error: Failed to load LTB model!\n");
		}
		else if (ret == CONVERT_RET_DECODING_FAILED)
		{
			printf("Error: Failed to decompress LTB file!\n");
		}
		printf("LTB conversion failed!\n");
	}
	else
	{
		printf("LTB conversion successful!\n");
	}
}
#endif // ENABLE_LITHTECH

void recurseAndCollectFilePath(std::filesystem::path start, FilePathVec* filesVec)
{
	std::filesystem::directory_iterator end;
	std::filesystem::directory_iterator dirIt(start);
	for (dirIt; dirIt != end; ++dirIt)
	{
		std::filesystem::path p = *dirIt;
		//
		if (std::filesystem::is_directory(p))
		{
			recurseAndCollectFilePath(p, filesVec);
		}
		else 
		{
			std::filesystem::path ext = p.extension();
			if (ext == ".ltb" || ext == ".LTB" ||
				ext == ".dtx" || ext == ".DTX" ||
				ext == ".rez" || ext == ".REZ" ||
                ext == ".pak" || ext == ".PAK" ||
                ext == ".unity3d" || ext == ".UNITY3D" ||
                ext == ".bundle" || ext == ".BUNDLE" ||
                ext == ".assets" || ext == ".ASSETS")
			{
				filesVec->push_back(p.string());
			}
		}
	}
}
bool ExtractPAK(const std::string& pakPath, const std::string& outDir)
{
    namespace fs = std::filesystem;

    printf("Extracting PAK: %s\n", pakPath.c_str());

    fs::path sourceFile = pakPath;
    auto [bFileRead, vDataBuf] =
        ReadFileToBuffer(sourceFile.generic_string());

    if (bFileRead == false)
    {
        printf("  Error: failed to read PAK file '%s'.\n",
                    sourceFile.string().c_str());
        return false;
    }

    PakFile pakFile(std::move(vDataBuf),
                    sourceFile.filename().generic_u16string());

    if (pakFile.ParseHeader() == false)
    {
        printf("  Error: invalid PAK header.\n");
        return false;
    }

    if (pakFile.ParseEntries() == false)
    {
        printf("  Error: failed to parse PAK entries.\n");
        return false;
    }

    fs::path outPath = outDir;
    size_t okCount = 0;

    for (const auto& entry : pakFile.GetEntries())
    {
        auto [unpacked, vOutBuf] = pakFile.UnpackEntry(entry);

        if (unpacked == false)
        {
            printf("  Failed to unpack entry '%s'\n",
                        String_UTF16toUTF8(entry.FilePath).c_str());
            continue;
        }

        fs::path outFilePath = outPath;
        outFilePath /= fs::path(entry.FilePath);

        fs::create_directories(outFilePath.parent_path());

        const bool writeRes =
            WriteBufferToFile(vOutBuf, outFilePath.string());

        if (writeRes == false)
        {
            printf("  Failed to write unpacked file to %s\n",
                        outFilePath.string().c_str());
            continue;
        }

        printf("  Wrote decrypted file to %s\n",
                    outFilePath.string().c_str());
        ++okCount;
    }

    printf("PAK extraction finished. %zu entries written to %s\n",
                okCount, outDir.c_str());
    return okCount > 0;
}

int main(int argc,char** argv)
{

	if (argc <= 1) 
	{
		printf("****************************************************************\n");
		printf(" VortigauntTool - Command Line Interface\n");
		printf("****************************************************************\n");
		printf("\n");
		printf(" Usage: VortigauntTool [command] [input] [output] [options]\n");
		printf("\n");
		printf(" Archive Extract:\n");
		printf("    -multirezextract <folder>    Extract all .rez files recursively\n");
		printf("    -multipakextract <folder>    Extract all .pak (CSO) files recursively\n");
		printf("    -multixfsextract <folder>    Extract all .xfs (Xenesis) files recursively\n");
		printf("\n");
		printf(" Model Convert:\n");
		printf("    -gr2 <input.gr2> <output.smd>    Convert GR2 to SMD\n");
		printf("\n");
		printf(" Wav Convert :\n");
		printf("    -audio <input.mp3> <output.wav> [-rate 22050] [-bits 8]\n");
		printf("\n");
		printf(" WAD Operations:\n");
		printf("    -wadextract <input.wad> [output_dir]    Extract WAD textures\n");
		printf("    -wadcreate <folder> <output.wad>        Create WAD from images\n");
		printf("\n");
		printf(" Unity Operations:\n");
		printf("    -unity <input_file_or_dir> [output_dir] Extract Unity meshes, textures, and animation clips\n");
		printf("\n");
		printf(" LTB Options:\n");
		printf("    -ignoreMeshes       Do not export mesh data\n");
		printf("    -ignoreAnimations   Do not export animations\n");
		printf("\n");
		printf(" Drag & Drop: Drag .ltb, .dtx, .rez, .pak, .xfs, .unity3d, .bundle, .assets files or folders\n");
		printf("****************************************************************\n");
		return 0;
	}

	// Unity extraction command
	if (argc >= 2 && std::string(argv[1]) == "-unity")
	{
		if (argc < 3)
		{
			printf("Usage: VortigauntTool -unity <input_file_or_dir> [output_dir]\n");
			return 1;
		}
		std::string inputPath = argv[2];
		std::string outDir = (argc >= 4) ? argv[3] : 
			(std::filesystem::current_path() / "VortigauntUnityExtracted").string();
		
		std::filesystem::create_directories(outDir);
		
		printf("Processing Unity assets: %s -> %s\n", inputPath.c_str(), outDir.c_str());
		
		UnityPorter porter;
		porter.Process(inputPath, outDir);
		return 0;
	}

	if (argc >= 2 && std::string(argv[1]) == "-multirezextract")
	{
		std::filesystem::path rootDir;
		if (argc >= 3)
		{
			rootDir = std::filesystem::path(argv[2]);
		}
		else
		{
			rootDir = std::filesystem::current_path();
		}

		FilePathVec rezFiles;
		if (std::filesystem::is_directory(rootDir))
		{
			recurseAndCollectFilePath(rootDir, &rezFiles);
		}
		else
		{
			rezFiles.push_back(rootDir.string());
		}

		// Filter to only .rez files
		FilePathVec rezOnly;
		for (const auto& path : rezFiles)
		{
			if (grabFileExt(path) == "rez")
				rezOnly.push_back(path);
		}

		if (rezOnly.empty())
		{
			printf("No .rez files found under %s\n", rootDir.string().c_str());
			return 0;
		}

		std::filesystem::path baseOut = std::filesystem::current_path() / "VortigauntExtracted";
		std::filesystem::create_directories(baseOut);
		std::string outDir = baseOut.string();

		printf("Multi-REZ extract mode: root = %s, output = %s\n",
			   rootDir.string().c_str(), outDir.c_str());

		size_t okCount = 0;
		for (const auto& rezPath : rezOnly)
		{
			printf("\n----------------------------------------------\n");
			printf("Extracting REZ: %s\n", rezPath.c_str());
#ifdef ENABLE_LITHTECH
			if (g_rezExtractor->Load(rezPath) && g_rezExtractor->ExtractAll(outDir))
			{
				printf("  -> OK\n");
				++okCount;
			}
			else
			{
				printf("  -> FAILED\n");
			}
#else
			printf("  -> SKIPPED (Lithtech disabled)\n");
#endif
		}

		printf("\nMulti-REZ extraction finished. %zu/%zu REZ files extracted to %s\n",
			   okCount, rezOnly.size(), outDir.c_str());
		return 0;
	}

	// Multi-PAK extract mode
	if (argc >= 2 && std::string(argv[1]) == "-multipakextract")
	{
		std::filesystem::path rootDir;
		if (argc >= 3)
		{
			rootDir = std::filesystem::path(argv[2]);
		}
		else
		{
			rootDir = std::filesystem::current_path();
		}

		FilePathVec pakFiles;
		if (std::filesystem::is_directory(rootDir))
		{
			recurseAndCollectFilePath(rootDir, &pakFiles);
		}
		else
		{
			pakFiles.push_back(rootDir.string());
		}

		// Filter to only .pak files
		FilePathVec pakOnly;
		for (const auto& path : pakFiles)
		{
			if (grabFileExt(path) == "pak")
				pakOnly.push_back(path);
		}

		if (pakOnly.empty())
		{
			printf("No .pak files found under %s\n", rootDir.string().c_str());
			return 0;
		}

		std::filesystem::path baseOut = std::filesystem::current_path() / "VortigauntExtracted";
		std::filesystem::create_directories(baseOut);
		std::string outDir = baseOut.string();

		printf("Multi-PAK extract mode: root = %s, output = %s\n",
			   rootDir.string().c_str(), outDir.c_str());
		printf("Found %zu .pak files to extract.\n\n", pakOnly.size());

		size_t okCount = 0;
		for (const auto& pakPath : pakOnly)
		{
			printf("\n----------------------------------------------\n");
			if (ExtractPAK(pakPath, outDir))
			{
				printf("  -> OK\n");
				++okCount;
			}
			else
			{
				printf("  -> FAILED\n");
			}
		}

		printf("\n======================================================\n");
		printf("Multi-PAK extraction finished. %zu/%zu PAK files extracted to %s\n",
			   okCount, pakOnly.size(), outDir.c_str());
		return 0;
	}

	// Multi-XFS extract mode
	if (argc >= 2 && std::string(argv[1]) == "-multixfsextract")
	{
		std::filesystem::path rootDir;
		if (argc >= 3)
		{
			rootDir = std::filesystem::path(argv[2]);
		}
		else
		{
			rootDir = std::filesystem::current_path();
		}

		FilePathVec xfsFiles;
		if (std::filesystem::is_directory(rootDir))
		{
			recurseAndCollectFilePath(rootDir, &xfsFiles);
		}
		else
		{
			xfsFiles.push_back(rootDir.string());
		}

		FilePathVec xfsOnly;
		for (const auto& path : xfsFiles)
		{
			if (grabFileExt(path) == "xfs")
				xfsOnly.push_back(path);
		}

		if (xfsOnly.empty())
		{
			printf("No .xfs files found under %s\n", rootDir.string().c_str());
			return 0;
		}

		std::filesystem::path baseOut = std::filesystem::current_path() / "VortiGauntXfsExtracted";
		std::filesystem::create_directories(baseOut);
		std::string outDir = baseOut.string();

		printf("Multi-XFS extract mode: root = %s, output = %s\n",
			   rootDir.string().c_str(), outDir.c_str());
		printf("Found %zu .xfs files to extract.\n\n", xfsOnly.size());

		size_t okCount = 0;
		for (const auto& xfsPath : xfsOnly)
		{
			printf("\n----------------------------------------------\n");
			printf("Extracting XFS: %s\n", xfsPath.c_str());
			if (g_xfsExtractor->Load(xfsPath) && g_xfsExtractor->ExtractAll(outDir))
			{
				printf("  -> OK\n");
				++okCount;
			}
			else
			{
				printf("  -> FAILED\n");
			}
		}

		printf("\n======================================================\n");
		printf("Multi-XFS extraction finished. %zu/%zu XFS files extracted to %s\n",
			   okCount, xfsOnly.size(), outDir.c_str());
		return 0;
	}

	// GR2 conversion mode
	if (argc >= 2 && std::string(argv[1]) == "-gr2")
	{
#ifdef ENABLE_GRANNY2
		if (argc < 4)
		{
			printf("Usage: VortigauntTool -gr2 <input.gr2> <output.smd>\n");
			return 1;
		}
		std::string inputPath = argv[2];
		std::string outputPath = argv[3];
		
		printf("Converting GR2: %s -> %s\n", inputPath.c_str(), outputPath.c_str());
		
		int result = g_gr2Converter->ConvertSingleGr2File(inputPath, outputPath);
		if (result == GR2_CONVERT_RET_OK)
		{
			printf("GR2 conversion successful!\n");
		}
		else
		{
			printf("GR2 conversion failed (error code: %d)\n", result);
			return 1;
		}
		return 0;
#else
		printf("GR2 conversion not available (Granny2 SDK not enabled)\n");
		return 1;
#endif
	}

	// Audio conversion mode
	if (argc >= 2 && std::string(argv[1]) == "-audio")
	{
		if (argc < 4)
		{
			printf("Usage: VortigauntTool -audio <input.mp3|ogg> <output.wav> [-rate 22050] [-bits 8]\n");
			return 1;
		}
		std::string inputPath = argv[2];
		std::string outputPath = argv[3];
		
		AudioConvertSettings settings;
		settings.sampleRate = 22050;
		settings.bitDepth = 8;
		settings.channels = 1;
		
		// Parse optional arguments
		for (int i = 4; i < argc; i++)
		{
			std::string arg = argv[i];
			if (arg == "-rate" && i + 1 < argc)
			{
				settings.sampleRate = std::stoi(argv[++i]);
			}
			else if (arg == "-bits" && i + 1 < argc)
			{
				settings.bitDepth = static_cast<uint16_t>(std::stoi(argv[++i]));
			}
			else if (arg == "-stereo")
			{
				settings.channels = 2;
			}
		}
		
		printf("Converting audio: %s -> %s\n", inputPath.c_str(), outputPath.c_str());
		printf("  Sample rate: %u Hz, Bit depth: %u, Channels: %u\n",
			   settings.sampleRate, settings.bitDepth, settings.channels);
		
		AudioConvertResult result = g_audioConverter->convertToWav(inputPath, outputPath, settings);
		if (result == AUDIO_CONVERT_OK)
		{
			printf("Audio conversion successful!\n");
		}
		else
		{
			printf("Audio conversion failed: %s\n", AudioConverter::getResultString(result).c_str());
			return 1;
		}
		return 0;
	}

	// WAD extract mode
	if (argc >= 2 && std::string(argv[1]) == "-wadextract")
	{
		if (argc < 3)
		{
			printf("Usage: VortigauntTool -wadextract <input.wad> [output_dir]\n");
			return 1;
		}
		std::string wadPath = argv[2];
		std::string outDir = (argc >= 4) ? argv[3] : 
			(std::filesystem::current_path() / "VortiGauntWadExtracted").string();
		
		std::filesystem::create_directories(outDir);
		
		printf("Extracting WAD: %s -> %s\n", wadPath.c_str(), outDir.c_str());
		
		if (!g_wadArchive->load(wadPath))
		{
			printf("Failed to load WAD file\n");
			return 1;
		}
		
		size_t count = g_wadArchive->textureCount();
		printf("Found %zu textures\n", count);
		
		size_t okCount = 0;
		const auto& textures = g_wadArchive->textures();
		for (size_t i = 0; i < textures.size(); ++i)
		{
			std::string texName = textures[i].name;
			std::string outPath = outDir + "/" + texName + ".bmp";
			
			if (g_wadArchive->extractTextureToBmp(i, outPath))
			{
				printf("  Extracted: %s\n", texName.c_str());
				++okCount;
			}
			else
			{
				printf("  Failed: %s\n", texName.c_str());
			}
		}
		
		printf("\nWAD extraction finished. %zu/%zu textures extracted\n", okCount, count);
		return 0;
	}

	// WAD create mode
	if (argc >= 2 && std::string(argv[1]) == "-wadcreate")
	{
		if (argc < 4)
		{
			printf("Usage: VortigauntTool -wadcreate <input_folder> <output.wad>\n");
			return 1;
		}
		std::string inputFolder = argv[2];
		std::string outputWad = argv[3];
		
		if (!std::filesystem::is_directory(inputFolder))
		{
			printf("Error: %s is not a directory\n", inputFolder.c_str());
			return 1;
		}
		
		printf("Creating WAD from: %s -> %s\n", inputFolder.c_str(), outputWad.c_str());
		
		g_wadArchive->clear();
		
		size_t addedCount = 0;
		for (const auto& entry : std::filesystem::directory_iterator(inputFolder))
		{
			if (!entry.is_regular_file())
				continue;
			
			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			
			if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
			{
				std::string texName = entry.path().stem().string();
				if (g_wadArchive->addTextureFromImage(texName, entry.path().string()))
				{
					printf("  Added: %s\n", texName.c_str());
					++addedCount;
				}
				else
				{
					printf("  Failed to add: %s\n", texName.c_str());
				}
			}
		}
		
		if (addedCount == 0)
		{
			printf("No valid images found in folder\n");
			return 1;
		}
		
		if (g_wadArchive->save(outputWad))
		{
			printf("\nWAD created successfully with %zu textures\n", addedCount);
		}
		else
		{
			printf("\nFailed to save WAD file\n");
			return 1;
		}
		return 0;
	}

	std::string inFile = argv[1];
	bool isDir = std::filesystem::is_directory(inFile);
	//
	FilePathVec filesVec;
	if (isDir) 
	{
		std::filesystem::path src_path(inFile);
		recurseAndCollectFilePath(src_path, &filesVec);
		if (filesVec.size() <= 0)
		{
			printf("There is no file��.ltb or .dtx�� available to convert !\n");
			return 0;
		}
	}
	else 
	{
		filesVec.push_back(inFile);
	}
	//
	for (size_t i = 0; i < filesVec.size(); ++i) 
	{
		printf("\n\n");
		std::string inFormat = grabFileExt(filesVec[i]);
		if (inFormat == "dtx")
		{
#ifdef ENABLE_LITHTECH
			ConvertDTX(filesVec[i], inFormat, argc, argv);
#else
			printf("Lithtech SDK is not available on Linux. Cannot convert DTX.\\n");
#endif
		}
		else if (inFormat == "ltb")
		{
#ifdef  ENABLE_LITHTECH
			ConvertLTB(filesVec[i], inFormat, argc, argv);
#else
			printf("Lithtech SDK is not available on Linux. Cannot convert LTB.\\n");
#endif

		}
		else if (inFormat == "rez")
		{
			// Always extract all REZ contents into a single folder named
			// "VortigauntExtracted" in the current working directory.
			std::filesystem::path baseOut = std::filesystem::current_path() / "VortigauntExtracted";
			std::filesystem::create_directories(baseOut);
			std::string outDir = baseOut.string();

			printf("Extracting .REZ : %s -->> %s\n", filesVec[i].c_str(), outDir.c_str());
#ifdef ENABLE_LITHTECH
			if (g_rezExtractor->Load(filesVec[i]) && g_rezExtractor->ExtractAll(outDir))
			{
				printf("REZ extraction successful (directory table).\n");
			}
			else
			{
				printf("REZ parse failed...\n");
			}
#else
			printf("REZ extraction skipped (Lithtech SDK only working on Windows).\n");
#endif
		}
        else if (inFormat == "pak")
        {
            std::filesystem::path baseOut = std::filesystem::current_path() / "VortigauntExtracted";
            std::filesystem::create_directories(baseOut);
            std::string outDir = baseOut.string();

            if (!ExtractPAK(filesVec[i], outDir))
            {
                printf("PAK extract failed for %s\n", filesVec[i].c_str());
            }
        }
        else if (inFormat == "xfs")
        {
            std::filesystem::path baseOut = std::filesystem::current_path() / "VortigauntExtracted";
            std::filesystem::create_directories(baseOut);
            std::string outDir = baseOut.string();

            printf("Extracting .XFS : %s --> %s\n", filesVec[i].c_str(), outDir.c_str());
            if (g_xfsExtractor->Load(filesVec[i]) && g_xfsExtractor->ExtractAll(outDir))
            {
                printf("XFS extraction successful.\n");
            }
            else
            {
                printf("XFS extraction failed for %s\n", filesVec[i].c_str());
            }
        }
        else if (inFormat == "unity3d" || inFormat == "bundle" || inFormat == "assets")
        {
            std::filesystem::path baseOut = std::filesystem::current_path() / "VortigauntUnityExtracted";
            std::filesystem::create_directories(baseOut);
            std::string outDir = baseOut.string();

            printf("Extracting Unity assets : %s --> %s\n", filesVec[i].c_str(), outDir.c_str());
            UnityPorter porter;
            porter.Process(filesVec[i], outDir);
        }
		else
		{
			printf("Unsupported file type: %s\n", filesVec[i].c_str());
		}
	}
    return 0;   
}
