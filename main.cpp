#include "pch.h"
#include "main.h"

void ShowHelp()
{
    std::cout << "==========================================\n";
    std::cout << "  Asset Engine - Archive Manager v3\n";
    std::cout << "==========================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  AssetEngine.exe <command> [arguments]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  create <archive> <file1> [file2] [...]\n";
    std::cout << "      Create archive from list of files\n\n";
    std::cout << "  list <archive>\n";
    std::cout << "      Display archive contents with IDs\n\n";
    std::cout << "  extract <archive> <filename> <output>\n";
    std::cout << "      Extract specific file by name from archive\n\n";
    std::cout << "  extract-id <archive> <fileID> <output>\n";
    std::cout << "      Extract specific file by ID from archive\n\n";
    std::cout << "  extractall <archive> <outputdir>\n";
    std::cout << "      Extract all active files from archive\n\n";
    std::cout << "  add <archive> <filepath>\n";
    std::cout << "      Add file to existing archive (v3 mutable)\n\n";
    std::cout << "  remove <archive> <fileID>\n";
    std::cout << "      Mark file as deleted (soft delete)\n\n";
    std::cout << "  rename <archive> <fileID> <newname>\n";
    std::cout << "      Rename file in archive\n\n";
    std::cout << "  compact <archive>\n";
    std::cout << "      Remove deleted files and defragment archive\n\n";
    std::cout << "  validate <archive>\n";
    std::cout << "      Verify archive integrity\n\n";
    std::cout << "  help | -help\n";
    std::cout << "      Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  AssetEngine.exe create archive.asset file1.txt file2.png\n";
    std::cout << "  AssetEngine.exe list archive.asset\n";
    std::cout << "  AssetEngine.exe extract archive.asset file1.txt output.txt\n";
    std::cout << "  AssetEngine.exe add archive.asset newfile.dat\n";
    std::cout << "  AssetEngine.exe remove archive.asset 123456789012345\n";
    std::cout << "  AssetEngine.exe compact archive.asset\n";
    std::cout << "  AssetEngine.exe validate archive.asset\n";
    std::cout << "==========================================\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        ShowHelp();
        return 1;
    }

    std::string command = argv[1];

    if (command == "help" || command == "-help" || command == "--help" || command == "?")
    {
        ShowHelp();
        return 0;
    }

    if (command == "create")
    {
        if (argc < 4)
        {
            std::cerr << "Error: 'create' requires archive path and at least one file\n";
            std::cerr << "Usage: AssetEngine.exe create <archive> <file1> [file2] [...]\n";
            return 1;
        }

        std::string archivePath = argv[2];
        std::vector<std::string> files;

        for (int i = 3; i < argc; i++)
        {
            files.push_back(argv[i]);
        }

        std::cout << "Creating archive: " << archivePath << "\n";
        std::cout << "Files: " << files.size() << "\n";

        bool success = Archive::Create(archivePath, files);

        if (success)
        {
            std::cout << "Archive created successfully.\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to create archive.\n";
            return 1;
        }
    }

    if (command == "list")
    {
        if (argc < 3)
        {
            std::cerr << "Error: 'list' requires archive path\n";
            std::cerr << "Usage: AssetEngine.exe list <archive>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        bool success = Archive::List(archivePath);

        return success ? 0 : 1;
    }

    if (command == "extract")
    {
        if (argc < 5)
        {
            std::cerr << "Error: 'extract' requires archive, filename, and output path\n";
            std::cerr << "Usage: AssetEngine.exe extract <archive> <filename> <output>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        std::string filename = argv[3];
        std::string outputPath = argv[4];

        std::cout << "Extracting '" << filename << "' from " << archivePath << "\n";

        bool success = Archive::ExtractByName(archivePath, filename, outputPath);

        if (success)
        {
            std::cout << "File extracted to: " << outputPath << "\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to extract file.\n";
            return 1;
        }
    }

    if (command == "extract-id")
    {
        if (argc < 5)
        {
            std::cerr << "Error: 'extract-id' requires archive, file ID, and output path\n";
            std::cerr << "Usage: AssetEngine.exe extract-id <archive> <fileID> <output>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        UINT64 fileID = std::stoull(argv[3]);
        std::string outputPath = argv[4];

        std::cout << "Extracting file ID " << fileID << " from " << archivePath << "\n";

        bool success = Archive::Extract(archivePath, fileID, outputPath);

        if (success)
        {
            std::cout << "File extracted to: " << outputPath << "\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to extract file.\n";
            return 1;
        }
    }

    if (command == "extractall")
    {
        if (argc < 4)
        {
            std::cerr << "Error: 'extractall' requires archive and output directory\n";
            std::cerr << "Usage: AssetEngine.exe extractall <archive> <outputdir>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        std::string outputDir = argv[3];

        std::cout << "Extracting all files from " << archivePath << " to " << outputDir << "\n";

        bool success = Archive::ExtractAll(archivePath, outputDir);

        if (success)
        {
            std::cout << "All files extracted successfully.\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to extract files.\n";
            return 1;
        }
    }

    if (command == "validate")
    {
        if (argc < 3)
        {
            std::cerr << "Error: 'validate' requires archive path\n";
            std::cerr << "Usage: AssetEngine.exe validate <archive>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        bool success = Archive::Validate(archivePath);

        return success ? 0 : 1;
    }

    if (command == "add")
    {
        if (argc < 4)
        {
            std::cerr << "Error: 'add' requires archive path and file to add\n";
            std::cerr << "Usage: AssetEngine.exe add <archive> <filepath>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        std::string filePath = argv[3];
        UINT64 outFileID = 0;

        std::cout << "Adding '" << filePath << "' to " << archivePath << "\n";

        bool success = Archive::AddFile(archivePath, filePath, outFileID);

        if (success)
        {
            std::cout << "File added successfully with ID: " << outFileID << "\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to add file to archive.\n";
            return 1;
        }
    }

    if (command == "remove")
    {
        if (argc < 4)
        {
            std::cerr << "Error: 'remove' requires archive path and file ID\n";
            std::cerr << "Usage: AssetEngine.exe remove <archive> <fileID>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        UINT64 fileID = std::stoull(argv[3]);

        std::cout << "Marking file ID " << fileID << " as deleted in " << archivePath << "\n";

        bool success = Archive::RemoveFile(archivePath, fileID);

        if (success)
        {
            std::cout << "File marked as deleted successfully.\n";
            std::cout << "Use 'compact' command to reclaim space.\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to remove file from archive.\n";
            return 1;
        }
    }

    if (command == "rename")
    {
        if (argc < 5)
        {
            std::cerr << "Error: 'rename' requires archive path, file ID, and new name\n";
            std::cerr << "Usage: AssetEngine.exe rename <archive> <fileID> <newname>\n";
            return 1;
        }

        std::string archivePath = argv[2];
        UINT64 fileID = std::stoull(argv[3]);
        std::string newName = argv[4];

        std::cout << "Renaming file ID " << fileID << " to '" << newName << "' in " << archivePath << "\n";

        bool success = Archive::RenameFile(archivePath, fileID, newName);

        if (success)
        {
            std::cout << "File renamed successfully.\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to rename file.\n";
            return 1;
        }
    }

    if (command == "compact")
    {
        if (argc < 3)
        {
            std::cerr << "Error: 'compact' requires archive path\n";
            std::cerr << "Usage: AssetEngine.exe compact <archive>\n";
            return 1;
        }

        std::string archivePath = argv[2];

        std::cout << "Compacting archive: " << archivePath << "\n";
        std::cout << "This will permanently remove deleted files and defragment the archive.\n";

        bool success = Archive::Compact(archivePath);

        if (success)
        {
            std::cout << "Archive compacted successfully.\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to compact archive.\n";
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Use 'help' to see available commands.\n";
    return 1;
}

//#include "pch.h"
//#include "main.h"
//#include "File.h"
//
//void PrintBlobAsInts(const Blob& blob, const char* label)
//{
//    std::cout << label << " [Size: " << blob.GetSize() << ", Capacity: " << blob.GetCapacity() << "]: ";
//
//    UINT64 numInts = blob.GetSize() / sizeof(int);
//    for (UINT64 i = 0; i < numInts; i++)
//    {
//        const int* val = blob.At<int>(i);
//        if (val)
//            std::cout << *val << " ";
//    }
//    std::cout << std::endl;
//}
//
//// Helper pour v�rifier un test
//void AssertTrue(bool condition, const char* testName)
//{
//    if (condition)
//        std::cout << "[YES] " << testName << std::endl;
//    else
//        std::cout << "[NO] " << testName << " FAILED!" << std::endl;
//}
//
//int main()
//{
//    //File file;
//
//    //INT32 offset = 7;
//    //
//    //bool opened = file.Open("test.txt", WRITE);
//    //assert(opened && "Failed to open file for writing");
//    //
//    //const UINT8 content[] = "Noa le goooat";
//    //file.Write(content, sizeof(content) - 1, 1);
//    //
//    //UINT32 pos = file.Seek(offset);
//    //assert(pos == offset && "Seek did not move to expected position");
//    //
//    //file.Write(content, sizeof(content) - 1, 1);
//    //
//    //file.Close();
//    //
//    //bool reopened = file.Open("test.txt", READ);
//    //assert(reopened && "Failed to reopen file for reading");
//    //
//    //UINT32 endPos = file.Seek(0);
//    //assert(endPos == 0 && "Seek reset not behaving as expected");
//
//    //file.Close();
//
//    //=====Blob Tests=====
//    std::cout << "=== BLOB TEST SUITE ===" << std::endl << std::endl;
//
//    // ========================================
//    // TEST 1: Constructeur par d�faut
//    // ========================================
//    std::cout << "--- Test 1: Default Constructor ---" << std::endl;
//    {
//        Blob blob;
//        AssertTrue(blob.GetSize() == 0, "Size should be 0");
//        AssertTrue(blob.GetCapacity() == 256, "Capacity should be 256");
//        AssertTrue(blob.GetData() != nullptr, "Data pointer should not be null");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 2: Constructeur avec donn�es
//    // ========================================
//    std::cout << "--- Test 2: Constructor with Data ---" << std::endl;
//    {
//        UINT8 data[] = { 1, 2, 3, 4, 5 };
//        Blob blob(data, 5);
//        AssertTrue(blob.GetSize() == 5, "Size should be 5");
//        AssertTrue(blob.GetCapacity() == 5, "Capacity should be 5");
//        AssertTrue(blob.GetData()[0] == 1 && blob.GetData()[4] == 5, "Data should be copied");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 3: Append
//    // ========================================
//    std::cout << "--- Test 3: Append ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 10, 20, 30, 40, 50 };
//
//        bool success = blob.Append<int>(data, 5);
//        AssertTrue(success, "Append should succeed");
//        AssertTrue(blob.GetSize() == 20, "Size should be 20 bytes (5 ints)");
//        PrintBlobAsInts(blob, "After Append");
//
//        // Append encore
//        int moreData[] = { 60, 70 };
//        blob.Append<int>(moreData, 2);
//        AssertTrue(blob.GetSize() == 28, "Size should be 28 bytes (7 ints)");
//        PrintBlobAsInts(blob, "After second Append");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 4: Add au milieu
//    // ========================================
//    std::cout << "--- Test 4: Add (Insert) ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 10, 20, 30, 40, 50 };
//        blob.Append<int>(data, 5);
//        PrintBlobAsInts(blob, "Before Add");
//
//        // Ins�rer 99, 88 apr�s l'index 2 (entre 30 et 40)
//        int newData[] = { 99, 88 };
//        bool success = blob.Add<int>(newData, 2, 2);
//        AssertTrue(success, "Add should succeed");
//        AssertTrue(blob.GetSize() == 28, "Size should be 28 bytes (7 ints)");
//        PrintBlobAsInts(blob, "After Add at index 2");
//
//        // V�rifier l'ordre : [10, 20, 99, 88, 30, 40, 50]
//        AssertTrue(*blob.At<int>(0) == 10, "Index 0 should be 10");
//        AssertTrue(*blob.At<int>(2) == 99, "Index 2 should be 99");
//        AssertTrue(*blob.At<int>(4) == 30, "Index 4 should be 30");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 5: Add � la fin (devrait utiliser Append)
//    // ========================================
//    std::cout << "--- Test 5: Add at end (uses Append) ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 10, 20, 30 };
//        blob.Append<int>(data, 3);
//
//        int newData = 40;
//        bool success = blob.Add<int>(&newData, 3, 1);  // Index 3 = fin
//        AssertTrue(success, "Add at end should succeed");
//        PrintBlobAsInts(blob, "After Add at end");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 6: Remove
//    // ========================================
//    std::cout << "--- Test 6: Remove ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 10, 20, 30, 40, 50, 60, 70 };
//        blob.Append<int>(data, 7);
//        PrintBlobAsInts(blob, "Before Remove");
//
//        // Supprimer 2 �l�ments � l'index 2 (30 et 40)
//        bool success = blob.Remove<int>(2, 2);
//        AssertTrue(success, "Remove should succeed");
//        AssertTrue(blob.GetSize() == 20, "Size should be 20 bytes (5 ints)");
//        PrintBlobAsInts(blob, "After Remove");
//
//        // V�rifier : [10, 20, 50, 60, 70]
//        AssertTrue(*blob.At<int>(2) == 50, "Index 2 should be 50");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 7: At (acc�s direct)
//    // ========================================
//    std::cout << "--- Test 7: At (Direct Access) ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 100, 200, 300 };
//        blob.Append<int>(data, 3);
//
//        int* val = blob.At<int>(1);
//        AssertTrue(val != nullptr && *val == 200, "At(1) should return 200");
//
//        // Modifier via At
//        *val = 999;
//        AssertTrue(*blob.At<int>(1) == 999, "Value should be modified to 999");
//        PrintBlobAsInts(blob, "After modification via At");
//
//        // Test const version
//        const Blob& constBlob = blob;
//        const int* constVal = constBlob.At<int>(1);
//        AssertTrue(constVal != nullptr && *constVal == 999, "Const At should work");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 8: GetData
//    // ========================================
//    std::cout << "--- Test 8: GetData ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 1, 2, 3 };
//        blob.Append<int>(data, 3);
//
//        UINT8* rawData = blob.GetData();
//        AssertTrue(rawData != nullptr, "GetData should return non-null");
//
//        // Modifier via GetData
//        int* intPtr = reinterpret_cast<int*>(rawData);
//        intPtr[0] = 777;
//        AssertTrue(*blob.At<int>(0) == 777, "Modification via GetData should work");
//
//        // Test const version
//        const Blob& constBlob = blob;
//        const UINT8* constData = constBlob.GetData();
//        AssertTrue(constData != nullptr, "Const GetData should work");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 9: Clear
//    // ========================================
//    std::cout << "--- Test 9: Clear ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 10, 20, 30 };
//        blob.Append<int>(data, 3);
//        UINT64 capacityBefore = blob.GetCapacity();
//
//        blob.Clear();
//        AssertTrue(blob.GetSize() == 0, "Size should be 0 after Clear");
//        AssertTrue(blob.GetCapacity() == capacityBefore, "Capacity should remain unchanged");
//
//        // R�utiliser apr�s Clear
//        blob.Append<int>(data, 3);
//        AssertTrue(blob.GetSize() == 12, "Should be able to append after Clear");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 10: Reserve
//    // ========================================
//    std::cout << "--- Test 10: Reserve ---" << std::endl;
//    {
//        Blob blob;
//        AssertTrue(blob.GetCapacity() == 256, "Initial capacity 256");
//
//        bool success = blob.Reserve(1000);
//        AssertTrue(success, "Reserve should succeed");
//        AssertTrue(blob.GetCapacity() == 1000, "Capacity should be 1000");
//        AssertTrue(blob.GetSize() == 0, "Size should still be 0");
//
//        // Reserve avec valeur plus petite (ne devrait rien faire)
//        blob.Reserve(500);
//        AssertTrue(blob.GetCapacity() == 1000, "Capacity should remain 1000");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 11: Shrink
//    // ========================================
//    std::cout << "--- Test 11: Shrink ---" << std::endl;
//    {
//        Blob blob;
//        int data[1000];
//        for (int i = 0; i < 1000; i++) data[i] = i;
//
//        blob.Append<int>(data, 1000);
//        std::cout << "After append 1000 ints - Capacity: " << blob.GetCapacity() << std::endl;
//
//        blob.Remove<int>(0, 900);
//        std::cout << "After removing 900 ints - Size: " << blob.GetSize() << ", Capacity: " << blob.GetCapacity() << std::endl;
//
//        bool success = blob.Shrink();
//        AssertTrue(success, "Shrink should succeed");
//        AssertTrue(blob.GetCapacity() == blob.GetSize(), "Capacity should equal Size after Shrink");
//        std::cout << "After Shrink - Size: " << blob.GetSize() << ", Capacity: " << blob.GetCapacity() << std::endl;
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 12: Gestion d'erreurs
//    // ========================================
//    std::cout << "--- Test 12: Error Handling ---" << std::endl;
//    {
//        Blob blob;
//        int data[] = { 10, 20, 30 };
//        blob.Append<int>(data, 3);
//
//        // Append avec nullptr
//        bool success = blob.Append<int>(nullptr, 5);
//        AssertTrue(!success, "Append with nullptr should fail");
//
//        // Add hors limites
//        success = blob.Add<int>(data, 100, 1);
//        AssertTrue(!success, "Add out of bounds should fail");
//
//        // Remove hors limites
//        success = blob.Remove<int>(100, 1);
//        AssertTrue(!success, "Remove out of bounds should fail");
//
//        // At hors limites
//        int* ptr = blob.At<int>(100);
//        AssertTrue(ptr == nullptr, "At out of bounds should return nullptr");
//    }
//    std::cout << std::endl;
//
//    // ========================================
//    // TEST 13: Croissance dynamique
//    // ========================================
//    std::cout << "--- Test 13: Dynamic Growth ---" << std::endl;
//    {
//        Blob blob;
//        std::cout << "Initial capacity: " << blob.GetCapacity() << std::endl;
//
//        // Ajouter beaucoup de donn�es pour forcer plusieurs reallocs
//        for (int i = 0; i < 100; i++)
//        {
//            blob.Append<int>(&i, 1);
//        }
//
//        std::cout << "After 100 appends - Size: " << blob.GetSize() << ", Capacity: " << blob.GetCapacity() << std::endl;
//        AssertTrue(blob.GetSize() == 400, "Size should be 400 bytes");
//        AssertTrue(blob.GetCapacity() >= 400, "Capacity should be at least 400");
//
//        // V�rifier que les donn�es sont intactes
//        bool dataIntact = true;
//        for (int i = 0; i < 100; i++)
//        {
//            if (*blob.At<int>(i) != i)
//            {
//                dataIntact = false;
//                break;
//            }
//        }
//        AssertTrue(dataIntact, "Data should remain intact after growth");
//    }
//    std::cout << std::endl;
//
//    std::cout << "=== ALL TESTS COMPLETED ===" << std::endl;
//
//    return 0;
//}
