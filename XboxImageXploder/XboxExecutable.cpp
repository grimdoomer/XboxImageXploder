/*
	XboxImageXploder - Utility for modifying xbox executables.
	
	XboxExecutable.h - Types and functions for parsing and modifying xbox executable files.

	Author - Grimdoomer
*/

#include "XboxExecutable.h"
#include <assert.h>

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
	BYTE abHeaderData[XBE_IMAGE_HEADER_MIN_SIZE];
	BYTE* pbBuffer = nullptr;

	// Open the image file for reading and writing.
	this->hFileHandle = CreateFileA(this->sFileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (this->hFileHandle == INVALID_HANDLE_VALUE)
	{
		// Failed to open the file.
		printf("Failed to open \"%s\": %d\n", this->sFileName.c_str(), GetLastError());
		return false;
	}

	// Check to make sure the file is large enough to be an executable.
	if (GetFileSize(this->hFileHandle, nullptr) < XBE_IMAGE_HEADER_MIN_SIZE)
	{
		// The file is too small to be a valid xbox executable.
		printf("File is too small to be valid!\n");
		return false;
	}

	// Read enough of the header to get the true size of the image headers.
	if (ReadFile(this->hFileHandle, abHeaderData, XBE_IMAGE_HEADER_MIN_SIZE, &BytesRead, nullptr) == FALSE || BytesRead != XBE_IMAGE_HEADER_MIN_SIZE)
	{
		// Failed to read the image header.
		printf("Failed to read image header!\n");
		goto Cleanup;
	}

	// Validate the size of the image header.
	XBE_IMAGE_HEADER* pTempHeader = (XBE_IMAGE_HEADER*)abHeaderData;
	if (pTempHeader->SizeOfImageHeader < XBE_IMAGE_HEADER_MIN_SIZE)
	{
		// Image header size is invalid.
		printf("Xbe image header size is invalid!\n");
		goto Cleanup;
	}

	// Allocate a buffer we can use to read the executable header.
	DWORD headersSize = pTempHeader->SizeOfHeaders;
	pbBuffer = (PBYTE)malloc(headersSize);
	if (pbBuffer == nullptr)
	{
		// Not enough memory for allocation.
		printf("Failed to allocate memory for header data!\n");
		return false;
	}

	// Seek back to the start of the image.
	SetFilePointer(this->hFileHandle, 0, nullptr, FILE_BEGIN);

	// Read the image header.
	if (ReadFile(this->hFileHandle, pbBuffer, headersSize, &BytesRead, nullptr) == FALSE || BytesRead != headersSize)
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

	// Check if there are import modules and if so read the import table.
	if (this->sHeader.ImportTableAddress > 0)
	{
		// Loop and read all the import table entries.
		XBE_IMAGE_IMPORT_DESCRIPTOR* pImportDescriptor = (XBE_IMAGE_IMPORT_DESCRIPTOR*)(pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, this->sHeader.ImportTableAddress));
		while (pImportDescriptor->ImageThunkData != 0)
		{
			// Save the import module name.
			WCHAR* pNamePtr = (WCHAR*)(pbBuffer + XBE_HEADER_OFFSET_OF(&this->sHeader, pImportDescriptor->ModuleNameAddress));
			this->mImportDirectory.emplace(pImportDescriptor->ImageThunkData, std::wstring(pNamePtr));

			// Next import entry.
			pImportDescriptor++;
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
	pNewSection->VirtualAddress = ALIGN_TO(pLastSection->VirtualAddress + pLastSection->VirtualSize, 4096);
	pNewSection->VirtualSize = ALIGN_TO(sectionSize, 4);
	pNewSection->RawAddress = ALIGN_TO(pLastSection->RawAddress + pLastSection->RawSize, 4096);
	pNewSection->RawSize = ALIGN_TO(sectionSize, 4);
	pNewSection->SectionNameReferenceCount = 0;

	// Save the section header name.
	this->vSectionHeaderNames.push_back(sectionName);

	// Some xbe files will contain the original PE headers and include that data and the logo bitmap into SizeOfHeaders. Others
	// don't and SizeOfHeaders does not include the size of the logo bitmap. To make things easier we set SizeOfHeaders to the absolute
	// maximum header size possible based on the virtual address of the first image section.
	this->sHeader.SizeOfHeaders = this->pSectionHeaders[0].VirtualAddress - this->sHeader.BaseAddress;

	// Check if the xbe has a valid PE header.
	bool hasPeHeaders = false;
	if (this->sHeader.PEBaseAddress > 0)
	{
		WORD wMagic = 0;

		// Seek to where the PE headers should start and check the magic value.
		SetFilePointer(this->hFileHandle, this->sHeader.PEBaseAddress - this->sHeader.BaseAddress, NULL, FILE_BEGIN);
		if (ReadFile(this->hFileHandle, &wMagic, 2, &BytesWritten, NULL) == FALSE || BytesWritten != 2)
		{
			// Failed to read PE header magic.
			printf("Failed to read PE header data!\n");
			return false;
		}

		// Check if the xbe contains the original PE headers.
		hasPeHeaders = wMagic == 'ZM';
	}

	// Calculate how much space we have to work with based on whether or not the image has a valid PE header.
	DWORD headerSizeRemaining = 0;
	DWORD logoBitmapEndOffset = (this->sHeader.LogoBitmapAddress - this->sHeader.BaseAddress) + this->sHeader.LogoBitmapSize;
	if (hasPeHeaders == true)
		headerSizeRemaining = (this->sHeader.PEBaseAddress - this->sHeader.BaseAddress) - logoBitmapEndOffset;
	else
		headerSizeRemaining = this->sHeader.SizeOfHeaders - logoBitmapEndOffset;

	// Calculate the expected size increase and check if there's enough room in the header. We add an additional 16 bytes here to account
	// for padding on data that has moved around and may increase/decrease in size (it's not very scientific and should be calculated in a
	// more accurate way).
	DWORD headerSizeRequired = ALIGN_TO(sizeof(XBE_IMAGE_SECTION_HEADER) + sectionName.length() + 16, 4);
	if (headerSizeRequired > headerSizeRemaining)
	{
		// Check if the image still has the PE headers and determine if discarding them will help.
		if (hasPeHeaders == true && this->sHeader.SizeOfHeaders - logoBitmapEndOffset >= headerSizeRequired)
		{
			// Discard the PE headers to make room for the new section headers.
			printf("Not enough space in XBE header to add new section data, PE headers will be discarded...\n");
			this->sHeader.PEBaseAddress = 0;
			hasPeHeaders = false;
		}
		else
		{
			// Not enough space remaining in the header to add a new section.
			printf("Not enough space in XBE header to add new section data! Adding a new section not possible!\n");
			return false;
		}
	}

	// Allocate a new buffer for the header data.
	BYTE *pbNewHeader = (PBYTE)malloc(this->sHeader.SizeOfHeaders);
	if (pbNewHeader == nullptr)
	{
		// Failed to allocate memory for new header buffer.
		printf("Failed to allocate memory for new header buffer!\n");
		return false;
	}

	// Initialize the new header buffer.
	memset(pbNewHeader, 0, this->sHeader.SizeOfHeaders);

	// Copy the xbe header to the new buffer.
	XBE_IMAGE_HEADER *pXbeHeader = (XBE_IMAGE_HEADER*)pbNewHeader;
	*pXbeHeader = this->sHeader;
	pXbeHeader->SizeOfImageHeader = this->sHeader.SizeOfImageHeader;

	// Copy the xbe certificate to the new buffer.
	XBE_IMAGE_CERTIFICATE *pCertificate = (XBE_IMAGE_CERTIFICATE*)ALIGN_TO(((PBYTE)pXbeHeader + pXbeHeader->SizeOfImageHeader), 4);
	*pCertificate = this->sCertificate;

	// Update certificate address.
	pXbeHeader->CertificateAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pCertificate);

	// Copy section headers to the new buffer.
	XBE_IMAGE_SECTION_HEADER *pSectionHeaders = (XBE_IMAGE_SECTION_HEADER*)ALIGN_TO((PBYTE)pCertificate + this->sCertificate.Size, 4);
	memcpy(pSectionHeaders, this->pSectionHeaders, sizeof(XBE_IMAGE_SECTION_HEADER) * pXbeHeader->NumberOfSections);

	// Update the section pointer to our new section.
	pNewSection = &pSectionHeaders[pXbeHeader->NumberOfSections - 1];

	// Update the section headers address.
	pXbeHeader->SectionHeadersAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pSectionHeaders);

	// Loop through all of the section headers and correct the section name addresses.
	WORD *pSharedPagePtr = (WORD*)ALIGN_TO(pSectionHeaders + pXbeHeader->NumberOfSections, 4);
	char *pNamePtr = (char*)ALIGN_TO(pSharedPagePtr + pXbeHeader->NumberOfSections + 1, 4);
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

	// Check if the module contains an import table.
	if (pXbeHeader->ImportTableAddress > 0)
	{
		// Update the import table address.
		pNamePtr = (char*)ALIGN_TO(pNamePtr, 4);
		pXbeHeader->ImportTableAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pNamePtr);
		pNamePtr += sizeof(XBE_IMAGE_IMPORT_DESCRIPTOR) * (this->mImportDirectory.size() + 1);

		// Loop and write module import data.
		XBE_IMAGE_IMPORT_DESCRIPTOR* pImportDescriptor = (XBE_IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)pXbeHeader + XBE_HEADER_OFFSET_OF(pXbeHeader, pXbeHeader->ImportTableAddress));
		for (auto iter = this->mImportDirectory.begin(); iter != this->mImportDirectory.end(); iter++)
		{
			// Write the import entry.
			pImportDescriptor->ImageThunkData = iter->first;
			pImportDescriptor->ModuleNameAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pNamePtr);

			// Write name to buffer.
			lstrcpyW((WCHAR*)pNamePtr, iter->second.c_str());
			pNamePtr += (iter->second.size() + 1) * sizeof(wchar_t);

			// Next import entry.
			pImportDescriptor++;
		}

		// Write a null entry to signal the end of the import table.
		pImportDescriptor->ImageThunkData = 0;
		pImportDescriptor->ModuleNameAddress = 0;
	}

	// Copy library versions to the new buffer.
	XBOX_LIBRARY_VERSION *pLibraryVersions = (XBOX_LIBRARY_VERSION*)ALIGN_TO(pNamePtr, 4);
	memcpy(pLibraryVersions, this->pLibraryVersions, sizeof(XBOX_LIBRARY_VERSION) * pXbeHeader->NumberOfLibraryVersions);

	// Update the library version addresses.
	pXbeHeader->LibraryVersionsAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pLibraryVersions);
	pXbeHeader->KernelLibraryVersionAddress += pXbeHeader->LibraryVersionsAddress;
	pXbeHeader->XAPILibraryVersionAddress += pXbeHeader->LibraryVersionsAddress;

	// Setup pointer to additional header data.
	BYTE *pbNextPointer = (PBYTE)(pLibraryVersions + pXbeHeader->NumberOfLibraryVersions);

	// Check the original size of the header to determine if it was possible to have library features or not.
	if (this->sHeader.SizeOfImageHeader > FIELD_OFFSET(XBE_IMAGE_HEADER, LibraryFeaturesAddress))
	{
		// Check if there are library features, and if so copy them to the new buffer.
		if (pXbeHeader->NumberOfLibraryFeatures > 0)
		{
			// Copy library features to the new buffer.
			XBOX_LIBRARY_VERSION *pLibraryFeatures = (XBOX_LIBRARY_VERSION*)ALIGN_TO(pLibraryVersions + pXbeHeader->NumberOfLibraryVersions, 4);
			memcpy(pLibraryFeatures, this->pLibraryFeatures, sizeof(XBOX_LIBRARY_VERSION) * pXbeHeader->NumberOfLibraryFeatures);

			// Update the library features address.
			pXbeHeader->LibraryFeaturesAddress = XBE_HEADER_ADDRESS_OF(pXbeHeader, pLibraryFeatures);
			pbNextPointer += (DWORD)(pXbeHeader->NumberOfLibraryFeatures * sizeof(XBOX_LIBRARY_VERSION));
		}
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

	// Update the image size.
	pXbeHeader->SizeOfImage += ALIGN_TO(pNewSection->VirtualSize, 4);

	// Check if we need to copy in the original PE headers.
	DWORD imageDataStart = FindImageDataStartOffset();
	if (hasPeHeaders == true)
	{
		// Seek to the start of the PE headers.
		DWORD peHeaderOffset = this->sHeader.PEBaseAddress - this->sHeader.BaseAddress;
		SetFilePointer(this->hFileHandle, peHeaderOffset, NULL, FILE_BEGIN);

		DWORD peHeadersSize = pXbeHeader->SizeOfHeaders - peHeaderOffset;
		BYTE* pNewPeHeaders = (BYTE*)pXbeHeader + (pXbeHeader->SizeOfHeaders - peHeadersSize);

		// Read the PE headers into the new header buffer.
		if (ReadFile(this->hFileHandle, pNewPeHeaders, peHeadersSize, &BytesWritten, NULL) == FALSE || BytesWritten != peHeadersSize)
		{
			// Failed to read in pe headers.
			printf("Failed to read original PE headers %d\n", GetLastError());
			return false;
		}

		// Update the PE headers address.
		pXbeHeader->PEBaseAddress = pXbeHeader->BaseAddress + (pXbeHeader->SizeOfHeaders - peHeadersSize);
	}

	// Seek to the beginning of the file and write the new image header.
	SetFilePointer(this->hFileHandle, 0, nullptr, FILE_BEGIN);
	if (WriteFile(this->hFileHandle, pbNewHeader, pXbeHeader->SizeOfHeaders, &BytesWritten, nullptr) == FALSE || BytesWritten != pXbeHeader->SizeOfHeaders)
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

DWORD XboxExecutable::FindImageDataStartOffset()
{
	DWORD lowestOffset = 0xFFFFFFFF;

	// Loop through all the sections and find the lowest image offset.
	for (int i = 0; i < this->sHeader.NumberOfSections; i++)
	{
		// Check if this section is the lowest we've seen so far.
		if (this->pSectionHeaders[i].RawAddress < lowestOffset)
			lowestOffset = this->pSectionHeaders[i].RawAddress;
	}

	return lowestOffset;
}