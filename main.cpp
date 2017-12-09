#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>

typedef unsigned long long QWORD;

#pragma pack(push, 1)

struct ArchiveHeader
{
	char Header[64];
	QWORD FilesCount;
};

struct ArchiveEntry
{
	char FileName[128];
	QWORD StartOffset;
	QWORD Length;
};

#pragma pack(pop)

#define SIG_PACKONLY	("PackOnly")
#define SIG_PACKPLUS	("PackPlus")

#define TYPE_PACKONLY	0
#define TYPE_PACKPLUS	1

#define SAFE_EXIT(x);	fclose(x); return 0;

inline BOOL dexist(const char* dir)
{
	struct stat buf;
	return (stat(dir, &buf) == 0 && buf.st_mode & S_IFDIR);
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("Usage: %s <filename> <out_directory>\n", argv[0]);
		return 0;
	}

	FILE* file = fopen(argv[1], "rb");

	if (file)
	{
		ArchiveHeader header;

		if (fread(&header, 1, sizeof(ArchiveHeader), file) != sizeof(ArchiveHeader))
		{
			printf("Can't read file header!\n");
			SAFE_EXIT(file);
		}

		BYTE archive_type = (strcmp(header.Header, SIG_PACKONLY) != 0);

		if (archive_type && strcmp(header.Header, SIG_PACKPLUS) != 0)
		{
			printf("This file is not a CC archive!\n");
			SAFE_EXIT(file);
		}

		if (header.FilesCount <= 0)
		{
			printf("There's no files in this archive!\n");
			SAFE_EXIT(file);
		}

		fseek(file, 0, SEEK_END);

		if (ftell(file) < (sizeof(ArchiveHeader) + sizeof(ArchiveEntry) * header.FilesCount))
		{
			printf("Bad header (invalid entry count)\n");
			SAFE_EXIT(file);
		}

		fseek(file, sizeof(ArchiveHeader), SEEK_SET);

		ArchiveEntry* arch_files = new ArchiveEntry[(DWORD)header.FilesCount];

		QWORD biggest_offset = 0;

		for (QWORD i = 0; i < header.FilesCount; i++)
		{
			if (fread(&arch_files[i], 1, sizeof(ArchiveEntry), file) != sizeof(ArchiveEntry))
			{
				printf("Can't read file entry (%llu)!\n", i);
				SAFE_EXIT(file);
			}

			QWORD sum = arch_files[i].StartOffset + arch_files[i].Length;

			if (sum > biggest_offset)
				biggest_offset = sum;
		}

		fseek(file, 0, SEEK_END);

		if (ftell(file) < biggest_offset)
		{
			printf("Bad header (invalid data block size)\n");
			SAFE_EXIT(file);
		}

		if (!dexist(argv[2]) && !CreateDirectory(argv[2], NULL))
		{
			printf("Unable to create output directory!\n");
			SAFE_EXIT(file);
		}

		BYTE brk = FALSE;

		char path[MAX_PATH + 1];

		for (QWORD i = 0; i < header.FilesCount; i++)
		{
			fseek(file, (long)arch_files[i].StartOffset, SEEK_SET);
			QWORD to_read = arch_files[i].Length;

			strcpy(path, argv[2]);
			strcat(path, "\\");
			strcat(path, arch_files[i].FileName);

			FILE* file_out = fopen(path, "wb");

			if (!file_out)
			{
				printf("Unable to create output file (%s)!", arch_files[i].FileName);
				SAFE_EXIT(file);
			}

			while (to_read > 0)
			{
				int c = fgetc(file);

				if (c == EOF)
				{
					brk = TRUE;
					break;
				}

				fputc((archive_type == TYPE_PACKONLY) ? c : ~c, file_out);

				to_read--;
			}

			fclose(file_out);

			if (brk) break;
		}

		if (brk)
		{
			printf("Unexpected end of file!\n");
			SAFE_EXIT(file);
		}

		fclose(file);
	}
	else printf("Can't find specified file!\n");

	return 0;
}