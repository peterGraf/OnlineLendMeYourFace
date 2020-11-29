/*
LendMeYourFace.c - Main file for Online Lend Me Your Face - a cgi-bin executable.

Copyright (C) 2020, Tamiko Thiel and Peter Graf - All Rights Reserved

   This file is part of Online Lend Me Your Face.
   The online handling of Lend Me Your Face is free software.

	This cgi-program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Arpoise is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Arpoise.  If not, see <https://www.gnu.org/licenses/>.

For more information on

Tamiko Thiel, see <https://www.TamikoThiel.com/>
Peter Graf, see <https://www.mission-base.com/peter/>
LendMeYourFace, see <https://www.tamikothiel.com/lendmeyourface/>

*/
/*
* Make sure "strings <exe> | grep Id | sort -u" shows the source file versions
*/
char* LendMeYourFace_c_id = "$Id: LendMeYourFace.c,v 1.5 2020/11/12 20:21:45 peter Exp $";

#include <stdio.h>
#include <memory.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include <assert.h>
#include <stdlib.h>

#ifdef _WIN32

#include <winsock2.h>
#include <direct.h>
#include <windows.h> 
#include <process.h>

#define socket_close closesocket

#else

#include <math.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define socket_close close

#ifndef h_addr
#define h_addr h_addr_list[0] /* for backward compatibility */
#endif

#endif

#include "pblCgi.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

static int timeStringLength = 12;
static int cookieLength = 36;
static char* cookie;

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
	struct timeval now;
	gettimeofday(&now, NULL);

	unsigned long duration = now.tv_sec * 1000000 + now.tv_usec;
	duration -= pblCgiStartTime.tv_sec * 1000000 + pblCgiStartTime.tv_usec;
	char* string = pblCgiSprintf("%lu", duration);
	pblCgiSetValue("executionDuration", string);
}

static void printTemplate(char* fileName, char* contentType)
{
	traceDuration();
	pblCgiPrint(getRequiredConfigValue("TemplateDirectoryPath"), fileName, contentType);
}

static int stringEndsWith(char* string, char* end)
{
	int length = strlen(string);
	int endLength = strlen(end);
	return length >= endLength && !strcmp(end, string + length - endLength);
}

static int fileNameMatches(char* name, char* pattern, char** extensions)
{
	if (!name || !*name)
	{
		return 0;
	}
	if (pattern && *pattern && !strstr(name, pattern))
	{
		return 0;
	}
	if (extensions)
	{
		char* extension;
		for (int i = 0; (extension = extensions[i]) && *extension; i++)
		{
			if (stringEndsWith(name, extension))
			{
				return 1;
			}
		}
		return 0;
	}
	return 1;
}

static PblList* listFilesInDirectory(char* path, char* pattern, char** extensions)
{
	char* tag = "ListDirectory";
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

		if (fileNameMatches(buffer, pattern, extensions))
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
		if (fileNameMatches(ent->d_name, pattern, extensions))
		{
			if (pblListAdd(list, pblCgiStrDup(ent->d_name)) < 0)
			{
				pblCgiExitOnError("%s: pbl_errno = %d, message='%s'\n", tag, pbl_errno, pbl_errstr);
			}
		}
	}
	closedir(dir);
#endif

	return list;
}

static char* getFileName(char* key)
{
	char* tag = "GetFileName";
	if (!cookie || strlen(cookie) != cookieLength)
	{
		pblCgiExitOnError("%s: This method needs a valid cookie, the invalid cookie is '%s'", tag, cookie ? cookie : "NULL");
		return NULL;
	}
	char* fileName = pblCgiQueryValue(key);
	if (!fileName || !*fileName)
	{
		pblCgiExitOnError("%s: Key '%s', query value for file name '%s' is empty", tag, key, fileName);
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
	if (strlen(fileName) != cookieLength)
	{
		pblCgiExitOnError("%s: Key '%s', expected %d bytes, received '%s', length %d", tag, key, cookieLength, fileName, strlen(fileName));
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

	char* publicVideosPath = getRequiredConfigValue("PublicVideosPath");
	if (!publicVideosPath)
	{
		return NULL;
	}
	char* publicVideosUrl = getRequiredConfigValue("PublicVideosUrl");
	if (!publicVideosUrl)
	{
		return NULL;
	}

	char* extensions[] = { ".mp4", (char*)NULL };
	PblList* videos = listFilesInDirectory(publicVideosPath, NULL, extensions);
	if (!videos || pblListIsEmpty(videos))
	{
		pblCgiExitOnError("%s: There are no videos in directory '%s'.\n", tag, publicVideosPath);
		return NULL;
	}
	unsigned int nVideos = pblListSize(videos);
	char* video = pblListGet(videos, rand() % nVideos);
	char* url = pblCgiStrCat(publicVideosUrl, video);

	PBL_CGI_TRACE("<<<< %s: Url '%s'", tag, url);
	return url;
}

static char* nextPrivateVideoUrl()
{
	char* tag = "NextPrivateVideoUrl";
	PBL_CGI_TRACE(">>>> %s", tag);

	char* outputVideosPath = getRequiredConfigValue("OutputVideosPath");
	if (!outputVideosPath)
	{
		return NULL;
	}
	char* outputVideosUrl = getRequiredConfigValue("OutputVideosUrl");
	if (!outputVideosUrl)
	{
		return NULL;
	}
	if (!cookie || strlen(cookie) != cookieLength)
	{
		PBL_CGI_TRACE("<<<< %s: Invalid cookie '%s'", tag, cookie ? cookie : "NULL");
		return NULL;
	}

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
		PBL_CGI_TRACE("<<<< %s: No videos for %s", tag, cookie + timeStringLength);
		return NULL;
	}
	unsigned int nVideos = pblListSize(videos);

	char* url = NULL;
	for (int i = 0; i < 100; i++)
	{
		int index = rand() % nVideos;
		char* video = pblListGet(videos, index);
		//PBL_CGI_TRACE(":::: %s, video %s", tag, video);

		char* txtFile = pblCgiStrReplace(video, ".mp4", ".txt");
		//PBL_CGI_TRACE(":::: %s, txtFile %s", tag, txtFile);

		if (pblCollectionContains(txtFiles, txtFile))
		{
			url = pblCgiStrCat(outputVideosUrl, video);
			break;
		}
	}

	PBL_CGI_TRACE("<<<< %s: Url '%s'", tag, url ? url : "");
	return url;
}

static int nextPublicVideo()
{
	char* tag = "NextPublicVideo";
	PBL_CGI_TRACE(">>> %s", tag);

	char* videoUrl = nextPrivateVideoUrl();
	if (videoUrl && *videoUrl)
	{
		pblCgiSetValue("HasPrivateVideo", "true");
	}
	videoUrl = nextPublicVideoUrl();
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
		videoUrl = nextPublicVideoUrl();
	}
	pblCgiSetValue("Video1", videoUrl);

	printTemplate("privateVideo.html", "text/html");
	PBL_CGI_TRACE("<<< %s: VideoUrl '%s'", tag, videoUrl);
	return 0;
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
	int N = imageWidth;

	// Loop to rotate the image 90 degree clockwise
	//
	for (int i = 0; i < N / 2; i++)
	{
		for (int j = i; j < N - i - 1; j++)
		{
			// Swap elements of each cycle in clockwise direction
			//
			//int temp = a[i][j];
			memcpy(temp, data + i * bytesPerLine + j * bytesPerPixel, bytesPerPixel);

			//a[i][j] = a[N - 1 - j][i];
			memcpy(data + i * bytesPerLine + j * bytesPerPixel, data + (N - 1 - j) * bytesPerLine + i * bytesPerPixel, bytesPerPixel);

			//a[N - 1 - j][i] = a[N - 1 - i][N - 1 - j];
			memcpy(data + (N - 1 - j) * bytesPerLine + i * bytesPerPixel, data + (N - 1 - i) * bytesPerLine + (N - 1 - j) * bytesPerPixel, bytesPerPixel);

			//a[N - 1 - i][N - 1 - j] = a[j][N - 1 - i];
			memcpy(data + (N - 1 - i) * bytesPerLine + (N - 1 - j) * bytesPerPixel, data + j * bytesPerLine + (N - 1 - i) * bytesPerPixel, bytesPerPixel);

			//a[j][N - 1 - i] = temp;
			memcpy(data + j * bytesPerLine + (N - 1 - i) * bytesPerPixel, temp, bytesPerPixel);
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

	char* convertedPicturesPath = getRequiredConfigValue("ConvertedPicturesPath");
	if (!convertedPicturesPath)
	{
		return -1;
	}
	char* inputPicturesPath = getRequiredConfigValue("InputPicturesPath");
	if (!inputPicturesPath)
	{
		return -1;
	}
	char* pauseFilePath = getRequiredConfigValue("PauseFilePath");
	if (!pauseFilePath)
	{
		return -1;
	}

	char* imageFileName = getFileName("imageFileName");
	if (!imageFileName)
	{
		return -1;
	}

	char* imageName = pblCgiSprintf("%s.png", imageFileName);
	char* imagePath = pblCgiSprintf("%s/%s", convertedPicturesPath, imageName);
	char* newPath = pblCgiSprintf("%s/%s", inputPicturesPath, imageName);

	if (rename(imagePath, newPath))
	{
		pblCgiExitOnError("%s: Failed to rename file '%s' to '%s'", tag, imagePath, newPath);
		return -1;
	}
	remove(pauseFilePath);

	PBL_CGI_TRACE("<<< %s: ", tag);

	return privateVideo();
}

static int deleteImage()
{
	char* tag = "DeleteImage";
	PBL_CGI_TRACE(">>> %s", tag);

	char* imageFileName = getFileName("imageFileName");
	if (!imageFileName)
	{
		return -1;
	}

	char* extensions[] = { ".png",  ".jpg", (char*)NULL };
	char* directories[3] = { getRequiredConfigValue("UploadedPicturesPath"), getRequiredConfigValue("ConvertedPicturesPath"),(char*)NULL };
	for (int i = 0; directories[i]; i++)
	{
		char* directory = directories[i];
		PblList* files = listFilesInDirectory(directory, imageFileName, extensions);
		if (files && !pblListIsEmpty(files))
		{
			int nFiles = pblListSize(files);
			for (int j = 0; j < nFiles; j++)
			{
				char* filePath = pblCgiSprintf("%s%s", directory, (char*)pblListGet(files, j));
				int rc = remove(filePath);
				PBL_CGI_TRACE("::: %s: removed '%s', rc %d", tag, filePath, rc);
			}
		}
	}
	printTemplate("startUpload.html", "text/html");
	PBL_CGI_TRACE("<<< %s: ", tag);
	return 0;
}

static int rotateImage()
{
	char* tag = "RotateImage";
	PBL_CGI_TRACE(">>> %s", tag);

	char* convertedPicturesPath = getRequiredConfigValue("ConvertedPicturesPath");
	if (!convertedPicturesPath)
	{
		return -1;
	}
	char* convertedPicturesUrl = getRequiredConfigValue("ConvertedPicturesUrl");
	if (!convertedPicturesUrl)
	{
		return -1;
	}

	char* imageFileName = getFileName("imageFileName");
	if (!imageFileName)
	{
		return -1;
	}

	char* imageName = pblCgiSprintf("%s.png", imageFileName);
	char* imagePath = pblCgiSprintf("%s/%s", convertedPicturesPath, imageName);
	char* imageUrl = pblCgiSprintf("%s/%s", convertedPicturesUrl, imageName);

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

	if (!cookie || strlen(cookie) != cookieLength)
	{
		pblCgiExitOnError("%s: This method needs a valid cookie, the invalid cookie is '%s'", tag, cookie ? cookie : "NULL");
		return -1;
	}

	char* uploadedPicturesPath = getRequiredConfigValue("UploadedPicturesPath");
	if (!uploadedPicturesPath)
	{
		return -1;
	}
	char* convertedPicturesPath = getRequiredConfigValue("ConvertedPicturesPath");
	if (!convertedPicturesPath)
	{
		return -1;
	}
	char* convertedPicturesUrl = getRequiredConfigValue("ConvertedPicturesUrl");
	if (!convertedPicturesUrl)
	{
		return -1;
	}

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
	char* uploadFilePath = pblCgiSprintf("%s/%s", uploadedPicturesPath, uploadFileName);

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
	char* imagePath = pblCgiSprintf("%s/%s", convertedPicturesPath, imageName);
	char* imageUrl = pblCgiSprintf("%s/%s", convertedPicturesUrl, imageName);

	convertFile(uploadFilePath, imagePath);

	pblCgiSetValue("imageFileName", imageFileName);
	pblCgiSetValue("imageName", imageName);
	pblCgiSetValue("imageUrl", imageUrl);

	printTemplate("confirmUpload.html", "text/html");
	PBL_CGI_TRACE("<<< %s: Image '%s', length %d", tag, imagePath, length);
	return 0;
}

static int setCookie()
{
	time_t now = time(NULL);
	char* timeString = pblCgiStrFromTimeAndFormat(now, "%02d%02d%02d%02d%02d%02d");

	char* remotePort = pblCgiGetEnv("REMOTE_PORT");
	if (remotePort && isdigit(*remotePort))
	{
		srand(rand() ^ atoi(remotePort));
	}
	int rand1 = rand();

	char* remoteIp = pblCgiGetEnv("REMOTE_ADDR");
	if (remoteIp && isdigit(*remoteIp))
	{
		srand(rand() ^ atoi(remoteIp));
	}
	int rand2 = rand();

	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	srand(rand() ^ currentTime.tv_sec ^ currentTime.tv_usec);
	int rand3 = rand();

	cookie = pblCgiSprintf("%s%08x%08x%08x", timeString, rand1, rand2, rand3);

	pblCgiSetValue(PBL_CGI_COOKIE, cookie);
	pblCgiSetValue(PBL_CGI_COOKIE_PATH, "/cgi-bin/");
	pblCgiSetValue(PBL_CGI_COOKIE_DOMAIN, "www.tamikothiel.com");

	printTemplate("startUpload.html", "text/html");
	return 0;
}

static int lendMeYourFace(int argc, char* argv[])
{
	int rc = 0;
	char* tag = "LendMeYourFace";

	struct timeval startTime;
	gettimeofday(&startTime, NULL);
	srand(rand() ^ startTime.tv_sec ^ startTime.tv_usec);

#ifdef _WIN32
	srand(rand() ^ _getpid());
	pblCgiConfigMap = pblCgiFileToMap(NULL, "../config/Win32LendMeYourFace.txt");

#else
	srand(rand() ^ getpid());
	pblCgiConfigMap = pblCgiFileToMap(NULL, "../config/LendMeYourFace.txt");

#endif

	char* traceFile = pblCgiConfigValue(PBL_CGI_TRACE_FILE, "/LendMeYourFace/LendMeYourFace.txt");
	if (!traceFile || !*traceFile)
	{
		pblCgiExitOnError("%s: Cannot determine 'PBL_CGI_TRACE_FILE'.\n", tag);
		return -1;
	}
	pblCgiInitTrace(&startTime, traceFile);
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

	// Read the cookie
	//
	cookie = pblCgiGetCoockie(pblCgiCookieKey, pblCgiCookieTag);

	// Read and handle the action
	//
	char* action = pblCgiQueryValue("action");
	if (!action || !*action)
	{
		rc = nextPublicVideo(); // There is no specific action, just show the next public video
	}
	else if (pblCgiStrEquals("setCookie", action))
	{
		rc = setCookie();
	}
	else if (!cookie || strlen(cookie) != cookieLength)
	{
		// All actions below need a valid cookie!
		//
		printTemplate("requestCookie.html", "text/html");
	}
	else if (pblCgiStrEquals("startUpload", action))
	{
		printTemplate("startUpload.html", "text/html");
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
	else if (pblCgiStrEquals("privateVideo", action))
	{
		rc = privateVideo();
	}
	else
	{
		rc = nextPublicVideo();
	}

	PBL_CGI_TRACE("<< %s: rc %d", tag, rc);
	return rc;
	}

int main(int argc, char* argv[])
{
	int rc = lendMeYourFace(argc, argv);
	if (rc)
	{
		pblCgiExitOnError("LendMeYourFace: Execution failed: %d\n", rc);
	}
	return rc;
}
