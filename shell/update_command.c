/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */
/*
 * start_command.c
 *
 *  Created on: Aug 20, 2010
 *      Author: alexanderb
 */
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "command_private.h"
#include "array_list.h"
#include "bundle_context.h"
#include "bundle.h"
#include "update_command.h"
#include "inputstream.h"

void updateCommand_execute(COMMAND command, char * line, void (*out)(char *), void (*err)(char *));
celix_status_t updateCommand_download(COMMAND command, char * url, char **inputFile);
size_t updateCommand_writeData(void *ptr, size_t size, size_t nmemb, FILE *stream);
int updateCommand_downloadProgress(void *ptr, double dltotal, double dlnow, double ultotal, double ulnow);

COMMAND updateCommand_create(BUNDLE_CONTEXT context) {
	COMMAND command = (COMMAND) malloc(sizeof(*command));
	command->bundleContext = context;
	command->name = "update";
	command->shortDescription = "update bundle.";
	command->usage = "update <id> [<URL>]";
	command->executeCommand = updateCommand_execute;
	return command;
}

void updateCommand_destroy(COMMAND command) {
	free(command);
}


void updateCommand_execute(COMMAND command, char * line, void (*out)(char *), void (*err)(char *)) {
	char delims[] = " ";
	char * sub = NULL;
	sub = strtok(line, delims);
	// Read bundle id
	sub = strtok(NULL, delims);
	if (sub != NULL) {
		long id = atol(sub);
		BUNDLE bundle = bundleContext_getBundleById(command->bundleContext, id);
		if (bundle != NULL) {
			sub = strtok(NULL, delims);
			char inputFile[MAXNAMLEN];
			inputFile[0] = '\0';
			if (sub != NULL) {
				printf("URL: %s\n", sub);
				char *test = inputFile;

				if (updateCommand_download(command, sub, &test) == CELIX_SUCCESS) {
					printf("Update bundle with stream\n");
					bundle_update(bundle, inputFile);
				} else {
					char error[256];
					sprintf(error, "Unable to download from %s\n", sub);
					err(error);
				}
			} else {
				bundle_update(bundle, NULL);
			}
		} else {
			err("Bundle id is invalid.\n");
		}
	}
}

celix_status_t updateCommand_download(COMMAND command, char * url, char **inputFile) {
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if (curl) {
		tmpnam(*inputFile);
		printf("Temp file: %s\n", *inputFile);
		FILE *fp = fopen(*inputFile, "wb+");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, updateCommand_writeData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		//curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
		//curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, updateCommand_downloadProgress);
		res = curl_easy_perform(curl);
		printf("Error: %d\n", res);
		/* always cleanup */
		curl_easy_cleanup(curl);
		fclose(fp);
	}
	if (res != CURLE_OK) {
		*inputFile[0] = '\0';
		return CELIX_ILLEGAL_STATE;
	} else {
		return CELIX_SUCCESS;
	}
}

size_t updateCommand_writeData(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

//int updateCommand_downloadProgress(void *ptr,
//                     double t, /* dltotal */
//                     double d, /* dlnow */
//                     double ultotal,
//                     double ulnow) {
//	printf("\r%f / %f (%g %%)", d, t, d*100.0/t);
//	return 0;
//}