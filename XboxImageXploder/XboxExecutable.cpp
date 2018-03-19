/*
	XboxImageXploder - Utility for modifying xbox executables.
	
	XboxExecutable.h - Types and functions for parsing and modifying xbox executable files.

	January 17th, 2018
		- Initial creation
*/

#include "XboxExecutable.h"

XboxExecutable::XboxExecutable(std::string fileName) : sFileName(), vSectionHeaderNames(), sDebugFullFileName(), sDebugFileNameUnicode()
{
	// Initialize fields.
	this->sFileName = fileName;
	this->hFileHandle = INVALID_HANDLE_VALUE;
	this->bIsValid = false;
}

XboxExecutable::~XboxExecutable()
{
	// Mark the object as invalid so no one else can use it.
	this->bIsValid = false;

	// Free buffers if allocated.
	if (this->pSectionHeaders)
	{
		free(this->pSectionHeaders);
	}

	if (this->pLibraryVersions)
	{
		free(this->pLibraryVersions);
	}

	if (this->pbLogoBitmap)
	{
		free(this->pbLogoBitmap);
	}

	// Check to see if the file handle is still open.
	if (this->hFileHandle != INVALID_HANDLE_VALUE)
	{
		// Close the file handle.
		CloseHandle(this->hFileHandle);
	}
}

bool XboxExecutable::ReadExecutable()
{
	bool result = false;
	DWORD BytesRead = 0;

	// Open the image file for reading and writing.
	this->hFileHandle = CreateFile(this->sFileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (this->hFileHandle == INVALID_HANDLE_VALUE)
	{
		// Failed to open the file.
		printf("Failed to open \"%s\": %d\n", this->sFileName.c_str(), GetLastError());
		return false;
	}

	// Check to make sure the file is large enough to be an executable.
	if (GetFileSize(this->hFileHandle, nullptr) < XBE_IMAGE_HEADER_MAX_SIZE)
	{
		// The file is too small to be a valid xbox executable.
		printf("File is too small to be valid!\n");
		return false;
	}

	// Allocate a buffer we can use to read the executable header.
	BYTE *pbBuffer = (PBYTE)malloc(XBE_IMAGE_HEADER_MAX_SIZE);
	if (pbBuffer == nullptr)
	{
		// Not enough memory for allocation.
		printf("Failed to allocate memory for header data!\n");
		return false;
	}

	// Read the image header.
	if (ReadFile(this->hFileHandle, pbBuffer, XBE_IMAGE_HEADER_MAX_SIZE, &BytesRead, nullptr) == FALSE || BytesRead != XBE_IMAGE_HEADER_MAX_SIZE)
	{
		// Failed to read the image header.
		printf("Failed to read image header!\n");
		goto Cleanup;
	}

	// Check if the xbe header is valid.
	this->sHeader = *(XBE_IMAGE_HEADER*)pbBuffer;
	if (this->sHeader.Magic != XBE_IMAGE_HEADER_MAGIC)
	{
		// Xbe header is invalid.
		printf("Xbe header has invalid magic!\n");
		goto Cleanup;
	}

	// Validate the size of the image header.
	if (this->sHeader.SizeOfImageHeader < XBE_IMAGE_HEADER_MIN_SIZE)
	{
		// Image header size is invalid.
		printf("Xbe image header size is invalid!\n");
		goto Cleanup;
	}

	// Clear additional fields if they are not used.
	memset((PBYTE)&this->sHeader + this->sHeader.SizeOfImageHeader, 0, sizeof(XBE_IMAGE_HEADER) - this->sHeader.SizeOfImageHeader);

	// Check the size of the certificate is valid.
	this->sCertificate = *(XBE_IMAGE_CERTIFICATE*)(pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.CertificateAddress));
	if (this->sCertificate.Size < XBE_IMAGE_CERTIFICATE_MIN_SIZE)
	{
		// Xbe certificate has invalid size.
		printf("Xbe certificate has invalid size!\n");
		goto Cleanup;
	}

	// Clear additional fields if they are not used.
	memset((PBYTE)&this->sCertificate + this->sCertificate.Size, 0, sizeof(XBE_IMAGE_CERTIFICATE) - this->sCertificate.Size);

	// Allocate memory for the section headers.
	this->pSectionHeaders = (XBE_IMAGE_SECTION_HEADER*)malloc(this->sHeader.NumberOfSections * sizeof(XBE_IMAGE_SECTION_HEADER));
	if (this->pSectionHeaders == nullptr)
	{
		// Failed to allocate memory for section headers.
		printf("Failed to allocate memory for section headers!\n");
		goto Cleanup;
	}

	// Loop and read all of the section headers.
	for (int i = 0; i < this->sHeader.NumberOfSections; i++)
	{
		// Read the section header.
		this->pSectionHeaders[i] = *(XBE_IMAGE_SECTION_HEADER*)(pbBuffer + 
			XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.SectionHeadersAddress) + (i * sizeof(XBE_IMAGE_SECTION_HEADER)));

		// Check if the section header has a name.
		if (this->pSectionHeaders[i].SectionNameAddress)
		{
			// Save the section header name.
			char *pNamePtr = (char*)pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, this->pSectionHeaders[i].SectionNameAddress);
			this->vSectionHeaderNames.push_back(std::string(pNamePtr));
		}
		else
		{
			// No name, this should never happen.
			this->vSectionHeaderNames.push_back(std::string());
		}
	}

	// Allocate memory for the library versions.
	this->pLibraryVersions = (XBOX_LIBRARY_VERSION*)malloc(this->sHeader.NumberOfLibraryVersions * sizeof(XBOX_LIBRARY_VERSION));
	if (this->pLibraryVersions == nullptr)
	{
		// Failed to allocate memory for library versions array.
		printf("Failed to allocate memory for library versions!\n");
		goto Cleanup;
	}

	// Loop and read all of the library versions.
	for (int i = 0; i < this->sHeader.NumberOfLibraryVersions; i++)
	{
		this->pLibraryVersions[i] = *(XBOX_LIBRARY_VERSION*)(pbBuffer +
			XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.LibraryVersionsAddress) + (i * sizeof(XBOX_LIBRARY_VERSION)));
	}

	// Check if there are library features and if so read them.
	if (this->sHeader.NumberOfLibraryFeatures > 0)
	{
		// Allocate memory for the library features.
		this->pLibraryFeatures = (XBOX_LIBRARY_VERSION*)malloc(this->sHeader.NumberOfLibraryFeatures * sizeof(XBOX_LIBRARY_VERSION));
		if (this->pLibraryFeatures == nullptr)
		{
			// Failed to allocate memory for library features array.
			printf("Failed to allocate memory for library features!\n");
			goto Cleanup;
		}

		// Loop and read all of the library features.
		for (int i = 0; i < this->sHeader.NumberOfLibraryFeatures; i++)
		{
			this->pLibraryFeatures[i] = *(XBOX_LIBRARY_VERSION*)(pbBuffer + 
				XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.LibraryFeaturesAddress) + (i * sizeof(XBOX_LIBRARY_VERSION)));
		}
	}

	// Check if the code view debug info address is valid.
	if (this->sHeader.CodeViewDebugInfoAddress != NULL)
	{
		// Not sure how to handle this yet...
		this->sHeader.CodeViewDebugInfoAddress = 0;
		//DebugBreak();
	}

	// Fixup library version addresses.
	if (this->sHeader.KernelLibraryVersionAddress)
		this->sHeader.KernelLibraryVersionAddress -= this->sHeader.LibraryVersionsAddress;
	if (this->sHeader.XAPILibraryVersionAddress)
		this->sHeader.XAPILibraryVersionAddress -= this->sHeader.LibraryVersionsAddress;

	// Read the debug file names.
	if (this->sHeader.FullFileNameAddress)	
		this->sDebugFullFileName = (CHAR*)(pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.FullFileNameAddress));
	if (this->sHeader.FileNameAddress)
		this->sHeader.FileNameAddress -= this->sHeader.FullFileNameAddress;
	if (this->sHeader.UnicodeFileNameAddress)
		this->sDebugFileNameUnicode = (WCHAR*)(pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.UnicodeFileNameAddress));

	// Allocate memory for the logo bitmap.
	this->pbLogoBitmap = (BYTE*)malloc(this->sHeader.LogoBitmapSize);
	if (this->pbLogoBitmap == nullptr)
	{
		// Failed to allocate memory for the logo bitmap.
		printf("Failed to allocate memory for the logo bitmap!\n");
		goto Cleanup;
	}

	// Copy the logo bitmap data.
	memcpy(this->pbLogoBitmap, pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.LogoBitmapAddress), this->sHeader.LogoBitmapSize);

	// Successfully read the image header.
	result = this->bIsValid = true;

Cleanup:
	// Free the temporary header buffer.
	free(pbBuffer);

	return result;
}

bool XboxExecutable::AddSectionForHacks(std::string sectionName, int sectionSize)
{
	DWORD BytesWritten = 0;

	// Check to make sure the executable was loaded and is valid.
	if (this->bIsValid == false)
		return false;

	// TODO: Check total header size to make sure there is enough space remaining for new section header + name.

	// Allocate a new array for the section headers.
	XBE_IMAGE_SECTION_HEADER *pNewSectionHeaders = (XBE_IMAGE_SECTION_HEADER*)malloc(sizeof(XBE_IMAGE_SECTION_HEADER) * (this->sHeader.NumberOfSections + 1));
	if (pNewSectionHeaders == nullptr)
	{
		// Failed to allocate memory for new section header array.
		printf("Failed to allocate memory for new section headers!\n");
		return false;
	}

	// Copy the old section headers into the new array.
	memcpy(pNewSectionHeaders, this->pSectionHeaders, this->sHeader.NumberOfSections * sizeof(XBE_IMAGE_SECTION_HEADER));
	
	// Free the old array and assign the new pointer.
	free(this->pSectionHeaders);
	this->pSectionHeaders = pNewSectionHeaders;
	this->sHeader.NumberOfSections += 1;

	// Pointers for easy access.
	XBE_IMAGE_SECTION_HEADER *pNewSection = &this->pSectionHeaders[this->sHeader.NumberOfSections - 1];
	XBE_IMAGE_SECTION_HEADER *pLastSection = &this->pSectionHeaders[this->sHeader.NumberOfSections - 2];

	// Initialize the new section header.
	memset(pNewSection, 0, sizeof(XBE_IMAGE_SECTION_HEADER));
	pNewSection->SectionFlags = (XBE_SECTION_FLAGS_WRITABLE | XBE_SECTION_FLAGS_PRELOAD | XBE_SECTION_FLAGS_EXECUTABLE);
	pNewSection->VirtualAddress = ALIGN_TO(pLastSection->VirtualAddress + pLastSection->VirtualSize, 16);
	pNewSection->VirtualSize = ALIGN_TO(sectionSize, 4);
	pNewSection->RawAddress = ALIGN_TO(pLastSection->RawAddress + pLastSection->RawSize, 0x1000);
	pNewSection->RawSize = ALIGN_TO(sectionSize, 4);
	pNewSection->SectionNameReferenceCount = 0;

	// Save the section header name.
	this->vSectionHeaderNames.push_back(sectionName);

	// Allocate a new buffer for the header data.
	BYTE *pbNewHeader = (PBYTE)malloc(XBE_IMAGE_HEADER_MAX_SIZE);
	if (pbNewHeader == nullptr)
	{
		// Failed to allocate memory for new header buffer.
		printf("Failed to allocate memory for new header buffer!\n");
		return false;
	}

	// Initialize the new header buffer.
	memset(pbNewHeader, 0, XBE_IMAGE_HEADER_MAX_SIZE);

	// Copy the xbe header to the new buffer.
	XBE_IMAGE_HEADER *pXbeHeader = (XBE_IMAGE_HEADER*)pbNewHeader;
	*pXbeHeader = this->sHeader;
	pXbeHeader->SizeOfImageHeader = sizeof(XBE_IMAGE_HEADER);

	// Copy the xbe certificate to the new buffer.
	XBE_IMAGE_CERTIFICATE *pCertificate = (XBE_IMAGE_CERTIFICATE*)ALIGN_TO(((PBYTE)pXbeHeader + this->sHeader.SizeOfImageHeader), 4);
	*pCertificate = this->sCertificate;

	// Update certificate address.
	pXbeHeader->CertificateAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pCertificate);

	// Copy section headers to the new buffer.
	XBE_IMAGE_SECTION_HEADER *pSectionHeaders = (XBE_IMAGE_SECTION_HEADER*)ALIGN_TO((PBYTE)pCertificate + this->sCertificate.Size, 4);
	memcpy(pSectionHeaders, this->pSectionHeaders, sizeof(XBE_IMAGE_SECTION_HEADER) * pXbeHeader->NumberOfSections);

	// Update the section headers address.
	pXbeHeader->SectionHeadersAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pSectionHeaders);

	// Loop through all of the section headers and correct the section name addresses.
	WORD *pSharedPagePtr = (WORD*)ALIGN_TO(pSectionHeaders + pXbeHeader->NumberOfSections, 4);
	char *pNamePtr = (char*)ALIGN_TO(pSharedPagePtr + pXbeHeader->NumberOfSections, 4);
	for (int i = 0; i < pXbeHeader->NumberOfSections; i++)
	{
		// Update the section header shared head/tail page address.
		pSectionHeaders[i].HeadSharedPageReferenceCount = XBE_HEADER_ADDRESS_OF(pXbeHeader, pSharedPagePtr + i);
		pSectionHeaders[i].TailSharedPageReferenceCount = XBE_HEADER_ADDRESS_OF(pXbeHeader, pSharedPagePtr + i + 1);

		// Update the section name address.
		pSectionHeaders[i].SectionNameAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pNamePtr);

		// Write name to buffer.
		strcpy(pNamePtr, this->vSectionHeaderNames.at(i).c_str());
		pNamePtr += this->vSectionHeaderNames.at(i).size() + 1;
	}

	// Copy library versions to the new buffer.
	XBOX_LIBRARY_VERSION *pLibraryVersions = (XBOX_LIBRARY_VERSION*)ALIGN_TO(pNamePtr, 4);
	memcpy(pLibraryVersions, this->pLibraryVersions, sizeof(XBOX_LIBRARY_VERSION) * pXbeHeader->NumberOfLibraryVersions);

	// Update the library version addresses.
	pXbeHeader->LibraryVersionsAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pLibraryVersions);
	pXbeHeader->KernelLibraryVersionAddress += pXbeHeader->LibraryVersionsAddress;
	pXbeHeader->XAPILibraryVersionAddress += pXbeHeader->LibraryVersionsAddress;

	// Check if there are library features, and if so copy them to the new buffer.
	BYTE *pbNextPointer = (PBYTE)(pLibraryVersions + pXbeHeader->NumberOfLibraryVersions);
	if (pXbeHeader->NumberOfLibraryFeatures > 0)
	{
		// Copy library features to the new buffer.
		XBOX_LIBRARY_VERSION *pLibraryFeatures = (XBOX_LIBRARY_VERSION*)ALIGN_TO(pLibraryVersions + pXbeHeader->NumberOfLibraryVersions, 4);
		memcpy(pLibraryFeatures, this->pLibraryFeatures, sizeof(XBOX_LIBRARY_VERSION) * pXbeHeader->NumberOfLibraryFeatures);

		// Update the library features address.
		pXbeHeader->LibraryFeaturesAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pLibraryFeatures);
		pbNextPointer += (DWORD)(pXbeHeader->NumberOfLibraryFeatures * sizeof(XBOX_LIBRARY_VERSION));
	}

	// Copy the debug file name (unicode).
	wchar_t *pDebugNameUnic = (wchar_t*)ALIGN_TO(pbNextPointer, 4);
	lstrcpyW(pDebugNameUnic, this->sDebugFileNameUnicode.c_str());

	// Copy the debug file name.
	char *pDebugFileName = (char*)ALIGN_TO(pDebugNameUnic + this->sDebugFileNameUnicode.size() + 1, 4);
	strcpy(pDebugFileName, this->sDebugFullFileName.c_str());

	// Update debug file name addresses.
	pXbeHeader->UnicodeFileNameAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pDebugNameUnic);
	pXbeHeader->FullFileNameAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pDebugFileName);
	pXbeHeader->FileNameAddress = pXbeHeader->FullFileNameAddress + (this->sDebugFullFileName.size() - this->sDebugFileNameUnicode.size());

	// Copy the logo bitmap data.
	BYTE *pbBitmapData = (BYTE*)ALIGN_TO(pDebugFileName + this->sDebugFullFileName.size() + 1, 4);
	memcpy(pbBitmapData, this->pbLogoBitmap, pXbeHeader->LogoBitmapSize);

	// Update the logo bitmap data address.
	pXbeHeader->LogoBitmapAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pbBitmapData);

	// Update the total header size.
	//pXbeHeader->SizeOfHeaders = ALIGN_TO(XBE_HEADER_OFFSET_OF(pXbeHeader, pXbeHeader->LogoBitmapAddress) + pXbeHeader->LogoBitmapSize, 4);
	pXbeHeader->SizeOfImage += ALIGN_TO(pNewSection->VirtualSize, 4);

	// Seek to the beginning of the file and write the new image header.
	SetFilePointer(this->hFileHandle, 0, nullptr, FILE_BEGIN);
	if (WriteFile(this->hFileHandle, pbNewHeader, XBE_IMAGE_HEADER_MAX_SIZE, &BytesWritten, nullptr) == FALSE || BytesWritten != XBE_IMAGE_HEADER_MAX_SIZE)
	{
		// Failed to write new image headers.
		printf("Failed to write new image headers to file!\n");
		return false;
	}

	// Allocate an empty buffer to write to the file for the new section.
	DWORD NewSectionSize = ALIGN_TO(sectionSize, 0x1000);
	BYTE *pbBlankData = (PBYTE)malloc(NewSectionSize);
	if (pbBlankData == nullptr)
	{
		// Failed to allocate blank data for new section.
		printf("Failed to allocate blank data for new section!\n");
		return false;
	}

	// Initialize the data to all 00s.
	memset(pbBlankData, 0, NewSectionSize);

	// Seek to the end of the file and write the blank data.
	SetFilePointer(this->hFileHandle, 0, nullptr, FILE_END);

	// Write the new section data.
	if (WriteFile(this->hFileHandle, pbBlankData, NewSectionSize, &BytesWritten, nullptr) == FALSE || BytesWritten != NewSectionSize)
	{
		// Failed to write new section data to the file.
		printf("Failed to write new section data to file!\n");
		return false;
	}

	// Print the new section info.
	printf("\nSection Name: \t\t%s\n", sectionName.c_str());
	printf("Virtual Address: \t0x%08x\n", pNewSection->VirtualAddress);
	printf("Virtual Size: \t\t0x%08x\n", pNewSection->VirtualSize);
	printf("File Offset: \t\t0x%08x\n", pNewSection->RawAddress);
	printf("File Size: \t\t0x%08x\n\n", pNewSection->RawSize);

	// Free temp buffers.
	free(pbBlankData);
	free(pbNewHeader);

	// Successfully added the new section to the image.
	return true;
}