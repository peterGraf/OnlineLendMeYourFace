/*
LendMeYourFace.c - Main file for online handling of Lend Me Your Face - a cgi-bin executable.

Copyright (C) 2020, Tamiko Thiel and Peter Graf - All Rights Reserved

   This file is part of the online handling of Lend Me Your Face.
   The online handling of Lend Me Your Face is free software.

	This cgi-program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this file.  If not, see <https://www.gnu.org/licenses/>.

For more information on

Tamiko Thiel, see <https://www.TamikoThiel.com/>
Peter Graf, see <https://www.mission-base.com/peter/>
LendMeYourFace, see <https://www.tamikothiel.com/lendmeyourface/>

*/
/*
* Make sure "strings <exe> | grep Id | sort -u" shows the source file versions
*/
char* LendMeYourFace_c_id = "$Id: LendMeYourFace.c,v 1.18 2020/12/19 20:21:45 peter Exp $";

#include "pblCgi.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

static const unsigned int timeStringLength = 12;
static const unsigned int expectedCookieLength = 44;
static char* cookie;
static int cookieLength;
static char* lastVideo;

static int setCookie(int clear)
{
	if (clear)
	{
		cookie = "";
		cookieLength = 0;
		pblCgiUnSetValue("HasCookie");
	}
	else
	{
		time_t now = time(NULL);
		char* timeString = pblCgiStrFromTimeAndFormat(now, "%02d%02d%02d%02d%02d%02d");
		if (timeString)
		{
			srand(pblCgiRand() ^ pblHtHashValueOfString((unsigned char*)timeString));
		}

		char* remotePort = pblCgiGetEnv("REMOTE_PORT");
		if (remotePort && isdigit(*remotePort))
		{
			srand(pblCgiRand() ^ atoi(remotePort));
		}
		int rand1 = pblCgiRand();

		char* remoteIp = pblCgiGetEnv("REMOTE_ADDR");
		if (remoteIp)
		{
			srand(pblCgiRand() ^ pblHtHashValueOfString((unsigned char*)remoteIp));
		}
		int rand2 = pblCgiRand();

		struct timeval currentTime;
		gettimeofday(&currentTime, NULL);
		srand(pblCgiRand() ^ currentTime.tv_sec ^ currentTime.tv_usec);
		int rand3 = pblCgiRand();

		char* userAgent = pblCgiGetEnv("HTTP_USER_AGENT");
		if (userAgent)
		{
			srand(pblCgiRand() ^ pblHtHashValueOfString((unsigned char*)userAgent));
		}
		int rand4 = pblCgiRand();

		cookie = pblCgiSprintf("%s%08x%08x%08x%08x", timeString, rand1, rand2, rand3, rand4);
		pblCgiSetValue("HasCookie", "true");
	}
	cookieLength = strlen(cookie);
	pblCgiSetValue(PBL_CGI_COOKIE, cookie);
	pblCgiSetValue(PBL_CGI_COOKIE_PATH, "/cgi-bin/");
	pblCgiSetValue(PBL_CGI_COOKIE_DOMAIN, "www.tamikothiel.com");

	return 0;
}

static void checkRequiredCookie(char* tag)
{
	char* lCookie = cookie;
	if (!lCookie || cookieLength != expectedCookieLength)
	{
		setCookie(1);
		pblCgiExitOnError("%s: This method needs a valid cookie, the invalid cookie is '%s'", tag, lCookie ? lCookie : "NULL");
	}
}

static char* getRequiredConfigValue(char* key)
{
	char* value = pblCgiConfigValue(key, NULL);
	if (!value || !*value)
	{
		pblCgiExitOnError("%s: Cannot find config value for key '%s'.\n", "GetRequiredConfigValue", key);
	}
	return value;
}

static void traceDuration()
{
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);

	unsigned long duration = currentTime.tv_sec * 1000000 + currentTime.tv_usec;
	duration -= pblCgiStartTime.tv_sec * 1000000 + pblCgiStartTime.tv_usec;
	char* string = pblCgiSprintf("%lu", duration);
	pblCgiSetValue("executionDuration", string);
}

static void printTemplate(char* fileName, char* contentType)
{
	traceDuration();
	pblCgiPrint(getRequiredConfigValue("TemplateDirectoryPath"), fileName, contentType);
}

static int stringEndsWith(char* string, int length, char* end)
{
	int endLength = strlen(end);
	return length >= endLength && !strcmp(end, string + length - endLength);
}

static int nameMatches(char* name, char* pattern, char** extensions)
{
	if (!name || !*name || (pattern && *pattern && !strstr(name, pattern)))
	{
		return 0;
	}
	if (extensions)
	{
		int length = strlen(name);
		char* extension;
		for (int i = 0; (extension = extensions[i]) && *extension; i++)
		{
			if (stringEndsWith(name, length, extension))
			{
				return 1;
			}
		}
		return 0;
	}
	return 1;
}

static int hasFilesInDirectory(char* path, char* pattern, char** extensions)
{
	char* tag = "HasFilesInDirectory";
	PBL_CGI_TRACE(">>>> %s: '%s', pattern '%s'", tag, path ? path : "", pattern ? pattern : "");
	int result = 0;
#ifdef _WIN32
	HANDLE hFind;
	WIN32_FIND_DATA findFileData;

	char* directoryPath = pblCgiSprintf("%s*.*", path);

	if ((hFind = FindFirstFileA(directoryPath, &findFileData)) == INVALID_HANDLE_VALUE)
	{
		pblCgiExitOnError("%s: Failed to access directory '%s', errno = %d\n", tag, path, GetLastError());
		return 0;
	}
	do
	{
		char buffer[1024];
		sprintf(buffer, "%ws", findFileData.cFileName);

		if (nameMatches(buffer, pattern, extensions))
		{
			result = 1;
			break;
		}
	} while (FindNextFile(hFind, &findFileData));
	FindClose(hFind);
#else
	DIR* dir;
	if ((dir = opendir(path)) == NULL)
	{
		pblCgiExitOnError("%s: Failed to access directory '%s', errno = %d\n", tag, path, errno);
		return 0;
	}

	struct dirent* ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (nameMatches(ent->d_name, pattern, extensions))
		{
			result = 1;
			break;
		}
	}
	closedir(dir);
#endif

	PBL_CGI_TRACE("<<<< %s: %d", tag, result);
	return result;
}

static PblList* listFilesInDirectory(char* path, char* pattern, char** extensions)
{
	char* tag = "ListFilesInDirectory";
	PBL_CGI_TRACE(">>>> %s: '%s', pattern '%s'", tag, path ? path : "", pattern ? pattern : "");

	PblList* list = pblListNewArrayList();
	if (!list)
	{
		pblCgiExitOnError("%s: pbl_errno = %d, message='%s'\n", tag, pbl_errno, pbl_errstr);
		return NULL;
	}
	pblCollectionSetCompareFunction(list, pblCollectionStringCompareFunction);

#ifdef _WIN32
	HANDLE hFind;
	WIN32_FIND_DATA findFileData;

	char* directoryPath = pblCgiSprintf("%s*.*", path);

	if ((hFind = FindFirstFileA(directoryPath, &findFileData)) == INVALID_HANDLE_VALUE)
	{
		pblCgiExitOnError("%s: Failed to access directory '%s', errno = %d\n", tag, path, GetLastError());
		return NULL;
	}
	do
	{
		char buffer[1024];
		sprintf(buffer, "%ws", findFileData.cFileName);

		if (nameMatches(buffer, pattern, extensions))
		{
			if (pblListAdd(list, pblCgiStrDup(buffer)) < 0)
			{
				pblCgiExitOnError("%s: pbl_errno = %d, message='%s'\n", tag, pbl_errno, pbl_errstr);
				return NULL;
			}
		}
	} while (FindNextFile(hFind, &findFileData));
	FindClose(hFind);
#else
	DIR* dir;
	if ((dir = opendir(path)) == NULL)
	{
		pblCgiExitOnError("%s: Failed to access directory '%s', errno = %d\n", tag, path, errno);
		return NULL;
	}

	struct dirent* ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (nameMatches(ent->d_name, pattern, extensions))
		{
			if (pblListAdd(list, pblCgiStrDup(ent->d_name)) < 0)
			{
				pblCgiExitOnError("%s: pbl_errno = %d, message='%s'\n", tag, pbl_errno, pbl_errstr);
			}
		}
	}
	closedir(dir);
#endif

	PBL_CGI_TRACE("<<<< %s: %d", tag, pblListSize(list));
	return list;
}

static char* getFileName(char* key)
{
	char* tag = "GetFileName";
	checkRequiredCookie(tag);

	char* fileName = pblCgiQueryValue(key);
	if (!fileName || !*fileName)
	{
		pblCgiExitOnError("%s: Key '%s', query value for file name '%s' is empty", tag, key, fileName ? fileName : "");
		return NULL;
	}
	for (char* ptr = fileName; *ptr; ptr++)
	{
		int c = *ptr;
		if (!(isdigit(c) || (islower(c) && isxdigit(c))))
		{
			*ptr = '\0';
			break;
		}
	}
	if (strlen(fileName) != expectedCookieLength)
	{
		pblCgiExitOnError("%s: Key '%s', expected %d bytes, received '%s', length %d", tag, key, expectedCookieLength, fileName, strlen(fileName));
	}
	if (strcmp(cookie + timeStringLength, fileName + timeStringLength))
	{
		pblCgiExitOnError("%s: The file name '%s' is not valid for you.", tag, fileName);
	}
	return fileName;
}

static char* nextPublicVideoUrl()
{
	char* tag = "NextPublicVideoUrl";
	PBL_CGI_TRACE(">>>> %s", tag);

	char* extensions[] = { ".mp4", (char*)NULL };
	PblList* videos = listFilesInDirectory(getRequiredConfigValue("PublicVideosPath"), NULL, extensions);
	if (!videos || pblListIsEmpty(videos))
	{
		pblCgiExitOnError("%s: There are no videos in directory '%s'.\n", tag, getRequiredConfigValue("PublicVideosPath"));
		return NULL;
	}
	char* url = NULL;
	int nVideos = pblListSize(videos);
	for (int i = 0; i < 1000; i++)
	{
		char* video = pblListGet(videos, pblCgiRand() % nVideos);
		if (nVideos > 1 && pblCgiStrEquals(lastVideo, video))
		{
			continue;
		}
		pblCgiSetValue("lastVideo", video);
	    url = pblCgiStrCat(getRequiredConfigValue("PublicVideosUrl"), video);
		break;
	}
	PBL_CGI_TRACE("<<<< %s: Url '%s'", tag, url);
	return url;
}

static int hasOutputVideo()
{
	char* tag = "hasOutputVideo";
	PBL_CGI_TRACE(">>>> %s", tag);

	if (!cookie || cookieLength != expectedCookieLength)
	{
		PBL_CGI_TRACE("<<<< %s: Invalid cookie '%s'", tag, cookie ? cookie : "NULL");
		return 0;
	}

	char* outputVideosPath = getRequiredConfigValue("OutputVideosPath");
	char* extensions[] = { ".mp4", (char*)NULL };
	int result = hasFilesInDirectory(outputVideosPath, cookie + timeStringLength, extensions);
	PBL_CGI_TRACE("<<<< %s: Result %d", tag, result);
	return result;
}

static char* nextPrivateVideoUrl()
{
	char* tag = "NextPrivateVideoUrl";
	PBL_CGI_TRACE(">>>> %s", tag);

	if (!cookie || cookieLength != expectedCookieLength)
	{
		PBL_CGI_TRACE("<<<< %s: Invalid cookie '%s'", tag, cookie ? cookie : "NULL");
		return NULL;
	}

	char* outputVideosPath = getRequiredConfigValue("OutputVideosPath");
	char* txtExtensions[] = { ".txt", (char*)NULL };
	PblList* txtFiles = listFilesInDirectory(outputVideosPath, cookie + timeStringLength, txtExtensions);
	if (!txtFiles || pblListIsEmpty(txtFiles))
	{
		PBL_CGI_TRACE("<<<< %s: No txt files for %s", tag, cookie + timeStringLength);
		return NULL;
	}

	char* extensions[] = { ".mp4", (char*)NULL };
	PblList* videos = listFilesInDirectory(outputVideosPath, cookie + timeStringLength, extensions);
	if (!videos || pblListIsEmpty(videos))
	{
		PBL_CGI_TRACE("<<<< %s: No mp4 videos for %s", tag, cookie + timeStringLength);
		return NULL;
	}

	char* url = NULL;
	int nVideos = pblListSize(videos);
	for (int i = 0; i < 1000; i++)
	{
		char* video = pblListGet(videos, pblCgiRand() % nVideos);
		char* txtFile = pblCgiStrReplace(video, ".mp4", ".txt");

		if (!pblCollectionContains(txtFiles, txtFile))
		{
			continue;
		}
		if (pblCgiStrEquals(lastVideo, video))
		{
			if (nVideos > 1)
			{
				continue;
			}
			break;
		}
		pblCgiSetValue("lastVideo", video);
		url = pblCgiStrCat(getRequiredConfigValue("OutputVideosUrl"), video);
		break;
	}
	PBL_CGI_TRACE("<<<< %s: Url '%s'", tag, url ? url : "");
	return url;
}

static int nextPublicVideo()
{
	char* tag = "NextPublicVideo";
	PBL_CGI_TRACE(">>> %s", tag);

	if (hasOutputVideo())
	{
		pblCgiSetValue("HasPrivateVideo", "true");
	}
	else
	{
		pblCgiUnSetValue("HasPrivateVideo");
	}
	char* videoUrl = nextPublicVideoUrl();
	pblCgiSetValue("Video1", videoUrl);
	printTemplate("index.html", "text/html");

	PBL_CGI_TRACE("<<< %s: VideoUrl '%s'", tag, videoUrl);
	return 0;
}

static int privateVideo()
{
	char* tag = "PrivateVideo";
	PBL_CGI_TRACE(">>> %s", tag);

	char* videoUrl = nextPrivateVideoUrl();
	if (videoUrl && *videoUrl)
	{
		pblCgiSetValue("HasPrivateVideo", "true");
	}
	else
	{
		pblCgiUnSetValue("HasPrivateVideo");
		videoUrl = nextPublicVideoUrl();
	}
	pblCgiSetValue("Video1", videoUrl);
	printTemplate("privateVideo.html", "text/html");

	PBL_CGI_TRACE("<<< %s: VideoUrl '%s'", tag, videoUrl);
	return 0;
}

static void removeFilesFromDirectories(char** directories, char* pattern, char** extensions)
{
	char* tag = "RemoveFilesFromDirectories";
	PBL_CGI_TRACE(">>>> %s: %s", tag, pattern ? pattern : NULL);

	if (!pattern || !*pattern)
	{
		pblCgiExitOnError("%s: Parameter 'pattern' cannot be empty", tag);
	}

	for (int i = 0; directories[i]; i++)
	{
		char* directory = directories[i];
		PblList* files = listFilesInDirectory(directory, pattern, extensions);
		if (files)
		{
			int nFiles = pblListSize(files);
			for (int j = 0; j < nFiles; j++)
			{
				char* filePath = pblCgiSprintf("%s%s", directory, (char*)pblListGet(files, j));
				int rc = remove(filePath);
				PBL_CGI_TRACE(":::: %s: removed '%s', rc %d", tag, filePath, rc);
			}
		}
	}
	PBL_CGI_TRACE("<<<< %s: ", tag);
	return;
}

static int deleteVideo()
{
	char* tag = "DeleteVideo";
	PBL_CGI_TRACE(">>> %s", tag);
	checkRequiredCookie(tag);

	char* videoName = pblCgiQueryValue("video");
	if (videoName && strlen(videoName) > expectedCookieLength
		&& !strncmp(videoName + timeStringLength, cookie + timeStringLength, expectedCookieLength - timeStringLength))
	{
		char* extensions[] = { ".txt", ".mp4", (char*)NULL };
		char* directories[2] = { getRequiredConfigValue("OutputVideosPath"), (char*)NULL };

		videoName = pblCgiStrReplace(videoName, ".mp4", "");
		removeFilesFromDirectories(directories, videoName, extensions);
	}
	int hasPrivateVideo = hasOutputVideo();

	PBL_CGI_TRACE("<<< %s: hasPrivateVideo %d", tag, hasPrivateVideo);
	return hasPrivateVideo ? privateVideo() : nextPublicVideo();
}

static int rotateFile(char* filePath)
{
	char* tag = "RotateFile";
	PBL_CGI_TRACE(">>>> %s", tag);

	int bytesPerPixel = 4;
	char temp[4];

	// Load the image to rotate
	//
	int imageWidth, imageHeight, imageBytesPerPixel;
	unsigned char* data = stbi_load(filePath, &imageWidth, &imageHeight, &imageBytesPerPixel, bytesPerPixel);
	if (!data)
	{
		pblCgiExitOnError("%s: Failed to read file '%s', reason '%s'", tag, filePath, stbi_failure_reason());
		return -1;
	}
	PBL_CGI_TRACE("%s: stbi_load(%s, %d, %d, %d, %d)", tag, filePath, imageWidth, imageHeight, imageBytesPerPixel, bytesPerPixel);

	if (imageWidth != imageHeight)
	{
		pblCgiExitOnError("%s: File '%s', is not square, width %d, height %d", tag, filePath, imageWidth, imageHeight);
		return -1;
	}
	int bytesPerLine = imageWidth * bytesPerPixel;

	for (int i = 0; i < imageWidth / 2; i++) // Loop to rotate the image 90 degree clockwise
	{
		int n1i = imageWidth - 1 - i;
		for (int j = i; j < imageWidth - i - 1; j++) // Swap elements of each cycle in clockwise direction
		{
			int n1j = imageWidth - 1 - j;
			memcpy(temp, data + i * bytesPerLine + j * bytesPerPixel, bytesPerPixel);
			memcpy(data + i * bytesPerLine + j * bytesPerPixel, data + n1j * bytesPerLine + i * bytesPerPixel, bytesPerPixel);
			memcpy(data + n1j * bytesPerLine + i * bytesPerPixel, data + n1i * bytesPerLine + n1j * bytesPerPixel, bytesPerPixel);
			memcpy(data + n1i * bytesPerLine + n1j * bytesPerPixel, data + j * bytesPerLine + n1i * bytesPerPixel, bytesPerPixel);
			memcpy(data + j * bytesPerLine + n1i * bytesPerPixel, temp, bytesPerPixel);
		}
	}

	int rc = stbi_write_png(filePath, imageWidth, imageWidth, bytesPerPixel, data, 0);
	if (rc != 1)
	{
		pblCgiExitOnError("%s: Failed to write png file '%s', reason '%s'", tag, filePath, stbi_failure_reason());
		return -1;
	}
	PBL_CGI_TRACE("%s: stbi_write_png(%s, %d, %d, %d)", tag, filePath, imageWidth, imageWidth, bytesPerPixel);
	PBL_CGI_TRACE("<<<< %s", tag);
	return 0;
}

static int convertFile(char* filePath, char* targetPath)
{
	char* tag = "ConvertFile";
	PBL_CGI_TRACE(">>>> %s", tag);

	int targetWidth = 512;
	int bytesPerPixel = 4;

	// Load the image to convert
	//
	int imageWidth, imageHeight, imageBytesPerPixel;
	unsigned char* data = stbi_load(filePath, &imageWidth, &imageHeight, &imageBytesPerPixel, bytesPerPixel);
	if (!data)
	{
		pblCgiExitOnError("%s: Failed to read file '%s', reason '%s'", tag, filePath, stbi_failure_reason());
		return -1;
	}
	PBL_CGI_TRACE("%s: stbi_load(%s, %d, %d, %d, %d)", tag, filePath, imageWidth, imageHeight, imageBytesPerPixel, bytesPerPixel);

	// Crop the image to a square if it is not already almost a square
	//
	int difference = imageWidth - imageHeight;
	difference = difference < 0 ? -difference : difference;

	int almostIsSquare = difference < 5;
	unsigned char* squareData = NULL;
	int squareWidth = imageWidth;
	int squareHeight = imageHeight;
	if (!almostIsSquare)
	{
		if (imageWidth < imageHeight)
		{
			squareHeight = imageWidth;
			int size = squareHeight * squareWidth * bytesPerPixel;
			unsigned char* to = squareData = (unsigned char*)pblCgiMalloc(tag, size);
			if (!squareData)
			{
				return -1;
			}
			unsigned char* from = data + bytesPerPixel * (difference / 2) * imageWidth;
			memcpy(to, from, size);
		}
		else
		{
			squareWidth = imageHeight;
			int size = squareHeight * squareWidth * bytesPerPixel;
			unsigned char* to = squareData = (unsigned char*)pblCgiMalloc(tag, size);
			if (!squareData)
			{
				return -1;
			}

			int bytesPerLine = squareWidth * bytesPerPixel;
			for (int i = 0; i < imageHeight; i++)
			{
				unsigned char* from = data + i * bytesPerPixel * imageWidth + bytesPerPixel * (difference / 2);
				memcpy(to, from, bytesPerLine);
				to += bytesPerLine;
			}
		}
	}
	else
	{
		squareData = data;
	}

	// Resize the square to targetWidth * targetWidth pixels
	//
	unsigned char* resizedData = (unsigned char*)pblCgiMalloc(tag, targetWidth * targetWidth * bytesPerPixel);
	if (!resizedData)
	{
		return -1;
	}

	int rc = stbir_resize_uint8(squareData, squareHeight, squareWidth, 0, resizedData, targetWidth, targetWidth, 0, bytesPerPixel);
	if (rc != 1)
	{
		pblCgiExitOnError("%s: Failed to resize data, reason '%s'", tag, stbi_failure_reason());
		return -1;
	}

	// Save the resized data
	//
	rc = stbi_write_png(targetPath, targetWidth, targetWidth, bytesPerPixel, resizedData, 0);
	if (rc != 1)
	{
		pblCgiExitOnError("%s: Failed to write png file '%s', reason '%s'", tag, targetPath, stbi_failure_reason());
		return -1;
	}
	PBL_CGI_TRACE("%s: stbi_write_png(%s, %d, %d, %d)", tag, targetPath, targetWidth, targetWidth, bytesPerPixel);
	PBL_CGI_TRACE("<<<< %s", tag);
	return 0;
}

static int useImage()
{
	char* tag = "UseImage";
	PBL_CGI_TRACE(">>> %s", tag);

	char* imageName = pblCgiSprintf("%s.png", getFileName("imageFileName"));
	char* imagePath = pblCgiSprintf("%s/%s", getRequiredConfigValue("ConvertedPicturesPath"), imageName);
	char* newPath = pblCgiSprintf("%s/%s", getRequiredConfigValue("InputPicturesPath"), imageName);

	if (rename(imagePath, newPath))
	{
		pblCgiExitOnError("%s: Failed to rename file '%s' to '%s'", tag, imagePath, newPath);
		return -1;
	}
	remove(getRequiredConfigValue("PauseFilePath"));

	PBL_CGI_TRACE("<<< %s: ", tag);
	return privateVideo();
}

static int deleteData()
{
	char* tag = "DeleteData";
	PBL_CGI_TRACE(">>> %s", tag);
	checkRequiredCookie(tag);

	char* extensions[] = { ".mp4", ".png",  ".jpg",  ".txt", (char*)NULL };
	char* directories[6] =
	{
		getRequiredConfigValue("UploadedPicturesPath"),
		getRequiredConfigValue("ConvertedPicturesPath"),
		getRequiredConfigValue("InputPicturesPath"),
		getRequiredConfigValue("HandledPicturesPath"),
		getRequiredConfigValue("OutputVideosPath"),
		(char*)NULL
	};
	removeFilesFromDirectories(directories, cookie + timeStringLength, extensions);

	PBL_CGI_TRACE("<<< %s: ", tag);
	return 0;
}

static int removeImage(char* pattern)
{
	char* tag = "RemoveImage";
	PBL_CGI_TRACE(">>> %s: '%s'", tag, pattern ? pattern : "");

	char* extensions[] = { ".png",  ".jpg",  ".txt", (char*)NULL };
	char* directories[5] =
	{
		getRequiredConfigValue("UploadedPicturesPath"),
		getRequiredConfigValue("ConvertedPicturesPath"),
		getRequiredConfigValue("HandledPicturesPath"),
		getRequiredConfigValue("InputPicturesPath"),
		(char*)NULL
	};
	removeFilesFromDirectories(directories, pattern, extensions);

	PBL_CGI_TRACE("<<< %s: ", tag);
	return 0;
}

static int deleteImage()
{
	char* tag = "DeleteImage";
	PBL_CGI_TRACE(">>> %s", tag);

	removeImage(getFileName("imageFileName"));
	printTemplate("startUpload.html", "text/html");

	PBL_CGI_TRACE("<<< %s: ", tag);
	return 0;
}

static int rotateImage()
{
	char* tag = "RotateImage";
	PBL_CGI_TRACE(">>> %s", tag);

	char* imageFileName = getFileName("imageFileName");
	if (!imageFileName)
	{
		return -1;
	}

	char* imageName = pblCgiSprintf("%s.png", imageFileName);
	char* imagePath = pblCgiSprintf("%s/%s", getRequiredConfigValue("ConvertedPicturesPath"), imageName);
	char* imageUrl = pblCgiSprintf("%s/%s", getRequiredConfigValue("ConvertedPicturesUrl"), imageName);

	rotateFile(imagePath);

	pblCgiSetValue("imageFileName", imageFileName);
	pblCgiSetValue("imageName", imageName);
	pblCgiSetValue("imageUrl", imageUrl);
	printTemplate("confirmUpload.html", "text/html");

	PBL_CGI_TRACE("<<< %s: ", tag);
	return 0;
}

static int uploadImage()
{
	char* tag = "UploadImage";
	PBL_CGI_TRACE(">>> %s", tag);
	checkRequiredCookie(tag);

	char* boundary = NULL;
	char* ptr = pblCgiGetEnv("CONTENT_TYPE");
	if (ptr && *ptr)
	{
		char* values[2 + 1];
		char* keyValuePair[2 + 1];

		int n = pblCgiStrSplit(ptr, " ", 2, values);
		if (n > 1)
		{
			if (pblCgiStrEquals("multipart/form-data;", values[0]))
			{
				pblCgiStrSplit(values[1], "=", 2, keyValuePair);
				if (pblCgiStrEquals("boundary", keyValuePair[0]))
				{
					boundary = keyValuePair[1];
				}
			}
		}
	}

	if (!boundary || !*boundary)
	{
		pblCgiExitOnError("%s: No multipart/form-data boundary defined\n", tag);
		return -1;
	}

	ptr = pblCgiPostData;
	if (!ptr || !*ptr)
	{
		pblCgiExitOnError("%s: No POST data defined\n", tag, boundary);
		return -1;
	}

	char* found = strstr(ptr, boundary);
	if (!found || !*found)
	{
		pblCgiExitOnError("%s: No multipart/form-data boundary '%s' found in post data '%s'\n", tag, boundary, ptr);
		return -1;
	}

	char* contentTypeTag = "Content-Type: ";
	char* contentType = strstr(pblCgiPostData, contentTypeTag);
	if (!contentType || !*contentType)
	{
		pblCgiExitOnError("%s: No Content-Type tag '%s' defined in post data %s\n", tag, contentTypeTag, pblCgiPostData);
		return -1;
	}
	contentType = contentType + strlen(contentTypeTag);
	if (!contentType || !*contentType)
	{
		pblCgiExitOnError("%s: No Content-Type value defined in post data %s\n", tag, pblCgiPostData);
		return -1;
	}
	ptr = contentType;
	while (!isspace(*ptr++));
	contentType = pblCgiStrRangeDup(contentType, ptr);
	if (!contentType || !*contentType)
	{
		pblCgiExitOnError("%s: No Content-Type value found in post data %s\n", tag, pblCgiPostData);
		return -1;
	}

	char* end = strstr(pblCgiPostData, "\r\n\r\n");
	if (!end)
	{
		end = strstr(ptr, "\n\n");
		if (!end)
		{
			pblCgiExitOnError("%s: Illegal header, terminating newlines are missing: '%s'\n", tag, ptr);
			return -1;
		}
		else
		{
			end += 2;
		}
	}
	else
	{
		end += 4;
	}
	ptr = end;
	int length = pblCgiContentLength - (ptr - pblCgiPostData) - strlen(boundary) - 6;

	char* extension = NULL;
	if (pblCgiStrEquals("image/png", contentType))
	{
		extension = "png";
	}
	else if (pblCgiStrEquals("image/jpeg", contentType))
	{
		extension = "jpg";
	}
	else
	{
		printTemplate("startUpload.html", "text/html");
		return 0;
	}

	time_t now = time(NULL);
	char* timeString = pblCgiStrFromTimeAndFormat(now, "%02d%02d%02d%02d%02d%02d");

	char* uploadFileName = pblCgiSprintf("%s%s.%s", timeString, cookie + timeStringLength, extension);
	char* uploadFilePath = pblCgiSprintf("%s/%s", getRequiredConfigValue("UploadedPicturesPath"), uploadFileName);

	FILE* stream;
	if (!(stream = pblCgiFopen(uploadFilePath, "w")))
	{
		pblCgiExitOnError("%s: Failed to open file '%s'", tag, uploadFilePath);
		return -1;
	}

	end = ptr + length;
	while (ptr < end)
	{
		fputc(*ptr++, stream);
	}
	fclose(stream);

	char* imageFileName = pblCgiSprintf("%s%s", timeString, cookie + timeStringLength);
	char* imageName = pblCgiSprintf("%s.png", imageFileName);
	char* imagePath = pblCgiSprintf("%s/%s", getRequiredConfigValue("ConvertedPicturesPath"), imageName);
	char* imageUrl = pblCgiSprintf("%s/%s", getRequiredConfigValue("ConvertedPicturesUrl"), imageName);

	convertFile(uploadFilePath, imagePath);

	pblCgiSetValue("imageFileName", imageFileName);
	pblCgiSetValue("imageName", imageName);
	pblCgiSetValue("imageUrl", imageUrl);
	printTemplate("confirmUpload.html", "text/html");

	PBL_CGI_TRACE("<<< %s: Image '%s', length %d", tag, imagePath, length);
	return 0;
}

static int isUploadPossible()
{
	char* tag = "IsUploadPossible";
	PBL_CGI_TRACE(">>> %s", tag);

	int rc = 1;
	int maxPictureQueueLength = 10;
	char* handledPicturesPath = getRequiredConfigValue("HandledPicturesPath");
	int handledPictures = 0;
	char* handledExtensions[] = { ".txt", (char*)NULL };
	PblList* handledFiles = listFilesInDirectory(handledPicturesPath, NULL, handledExtensions);
	if (handledFiles)
	{
		handledPictures = pblListSize(handledFiles);
		for (int i = 0; i < handledPictures; i++)
		{
			char* imageFileName = pblListGet(handledFiles, i);
			if (imageFileName)
			{
				imageFileName = pblCgiStrReplace(imageFileName, ".txt", "");
				removeImage(imageFileName);
			}
		}
	}

	char* value = pblCgiConfigValue("MaxPictureQueueLength", "10");
	if (value && isdigit(*value))
	{
		maxPictureQueueLength = atoi(value);
	}
	if (maxPictureQueueLength > 0)
	{
		handledPictures = 0;
		handledFiles = listFilesInDirectory(handledPicturesPath, NULL, handledExtensions);
		if (handledFiles)
		{
			handledPictures = pblListSize(handledFiles);
		}

		int inputPictures = 0;
		char* extensions[] = { ".png", (char*)NULL };
		PblList* inputFiles = listFilesInDirectory(getRequiredConfigValue("InputPicturesPath"), NULL, extensions);
		if (inputFiles)
		{
			inputPictures = pblListSize(inputFiles);
		}
		if (inputPictures - handledPictures > maxPictureQueueLength)
		{
			rc = 0;

			int estimatedMinutesForRender = 2;
			value = pblCgiConfigValue("EstimatedMinutesForRender", "2");
			if (value && isdigit(*value))
			{
				estimatedMinutesForRender = atoi(value);
			}
			pblCgiSetValue("MaxPictureQueueLength", pblCgiSprintf("%d", maxPictureQueueLength));
			pblCgiSetValue("PictureQueueLength", pblCgiSprintf("%d", inputPictures - handledPictures));
			pblCgiSetValue("EstimatedMinutesForRender", pblCgiSprintf("%d", (inputPictures - handledPictures) * estimatedMinutesForRender));
		}
	}
	PBL_CGI_TRACE("<<< %s: rc %d", tag, rc);
	return rc;
}

static int lendMeYourFace(int argc, char* argv[])
{
	int rc = 0;
	char* tag = "LendMeYourFace";

	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	srand(pblCgiRand() ^ currentTime.tv_sec ^ currentTime.tv_usec);

#ifdef _WIN32
	srand(pblCgiRand() ^ _getpid());
	pblCgiConfigMap = pblCgiFileToMap(NULL, "../config/Win32LendMeYourFace.txt");

#else
	srand(pblCgiRand() ^ getpid());
	pblCgiConfigMap = pblCgiFileToMap(NULL, "../config/LendMeYourFace.txt");

#endif

	char* traceFile = getRequiredConfigValue(PBL_CGI_TRACE_FILE);
	pblCgiInitTrace(&currentTime, traceFile);
	PBL_CGI_TRACE(">> %s: argc %d argv[0] = %s", tag, argc, argv[0]);

	pblCgiParseQuery(argc, argv);

#ifdef _WIN32
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		pblCgiExitOnError("%s: WSAStartup failed: %d\n", tag, result);
		return -1;
	}
#endif

	cookie = pblCgiGetCoockie(pblCgiCookieKey, pblCgiCookieTag);
	if (cookie && (cookieLength = strlen(cookie)) == expectedCookieLength)
	{
		pblCgiSetValue("HasCookie", "true");
	}

	lastVideo = pblCgiQueryValue("lastVideo");
	if (lastVideo && *lastVideo)
	{
		pblCgiSetValue("lastVideo", lastVideo);
	}

	char* action = pblCgiQueryValue("action");
	if (!action || !*action)
	{
		rc = nextPublicVideo(); // There is no specific action, default to the next public video
	}
	else if (pblCgiStrEquals("setCookie", action))
	{
		rc = setCookie(0);
		printTemplate("startUpload.html", "text/html");
	}
	else if (!cookie || cookieLength != expectedCookieLength)
	{
		if (pblCgiStrEquals("startUpload", action) && !isUploadPossible())
		{
			printTemplate("noUpload.html", "text/html"); // Explain that the upload is not possible
		}
		else
		{
			printTemplate("requestCookie.html", "text/html"); // All actions below need a valid cookie
		}
	}
	else if (pblCgiStrEquals("privateVideo", action))
	{
		rc = privateVideo();
	}
	else if (pblCgiStrEquals("startUpload", action))
	{
		printTemplate(isUploadPossible() ? "startUpload.html" : "noUpload.html", "text/html");
	}
	else if (pblCgiStrEquals("uploadImage", action))
	{
		rc = uploadImage();
	}
	else if (pblCgiStrEquals("rotateImage", action))
	{
		rc = rotateImage();
	}
	else if (pblCgiStrEquals("deleteImage", action))
	{
		rc = deleteImage();
	}
	else if (pblCgiStrEquals("useImage", action))
	{
		rc = useImage();
	}
	else if (pblCgiStrEquals("deleteVideo", action))
	{
		rc = deleteVideo();
	}
	else if (pblCgiStrEquals("deleteData", action))
	{
		rc = deleteData() || setCookie(1) || nextPublicVideo();
	}
	else
	{
		rc = nextPublicVideo(); // Show the next public video
	}
	PBL_CGI_TRACE("<< %s: rc %d", tag, rc);
	return rc;
}

int main(int argc, char* argv[])
{
	pblCgiSetSelfDestruction(2 * 60); // Terminate this program after some time

	int rc = lendMeYourFace(argc, argv);
	if (rc)
	{
		pblCgiExitOnError("LendMeYourFace: Execution failed: %d\n", rc);
	}
	return rc;
}
