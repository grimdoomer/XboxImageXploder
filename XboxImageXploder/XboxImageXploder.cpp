// XboxImageXploder.cpp : Defines the entry point for the console application.
//

#include <Windows.h>
#include <string>
#include "XboxExecutable.h"

void PrintUse()
{
	printf("XboxImageXploder.exe <xbe_file> <section_name> <section_size>\n\n");
}

int main(int argc, char **argv)
{
	// Check if the correct number of arguments were provided.
	if (argc != 4)
	{
		// Invalid number of arguments.
		PrintUse();
		return 0;
	}

	// Parse the arguments.
	std::string sFileName(argv[1]);
	std::string sSectionName(argv[2]);
	int sectionSize = atoi(argv[3]);

	// Create a new XboxExecutable object and try to read it.
	XboxExecutable *pXbe = new XboxExecutable(sFileName);
	if (pXbe->ReadExecutable() == false)
	{
		// Failed to read xbe.
		delete pXbe;
		return 0;
	}

	// Try to add the new section to the executable.
	if (pXbe->AddSectionForHacks(sSectionName, sectionSize) == false)
	{
		// Failed to add new section to the file.
		delete pXbe;
		return 0;
	}

	// Successfully added the new section.
	printf("Successfully added new section to image!\n");
    return 0;
}

