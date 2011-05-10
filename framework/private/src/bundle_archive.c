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
 * bundle_archive.c
 *
 *  Created on: Aug 8, 2010
 *      Author: alexanderb
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//#include <dirent.h>
//#include <sys/stat.h>

#include "bundle_archive.h"
#include "bundle_revision.h"
#include "headers.h"
#include "linked_list_iterator.h"

#include <apr-1/apr_file_io.h>

struct bundleArchive {
	long id;
	char * location;
	DIR * archiveRootDir;
	char * archiveRoot;
	LINKED_LIST revisions;
	long refreshCount;
	time_t lastModified;

	BUNDLE_STATE persistentState;

	apr_pool_t *mp;
};

BUNDLE_ARCHIVE bundleArchive_createSystemBundleArchive(apr_pool_t *mp) {
	BUNDLE_ARCHIVE archive = (BUNDLE_ARCHIVE) malloc(sizeof(*archive));
	archive->id = 0l;
	archive->location = "System Bundle";
	archive->mp = mp;
	archive->archiveRoot = NULL;
	archive->archiveRootDir = NULL;
	archive->revisions = linkedList_create();
	archive->refreshCount = -1;
	time(&archive->lastModified);

	return archive;
}

char * bundleArchive_getRevisionLocation(BUNDLE_ARCHIVE archive, long revNr);
void bundleArchive_setRevisionLocation(BUNDLE_ARCHIVE archive, char * location, long revNr);

void bundleArchive_initialize(BUNDLE_ARCHIVE archive);

void bundleArchive_deleteTree(char * directory, apr_pool_t *mp);

BUNDLE_REVISION bundleArchive_createRevisionFromLocation(BUNDLE_ARCHIVE archive, char *location, char *inputFile, long revNr);
void bundleArchive_reviseInternal(BUNDLE_ARCHIVE archive, bool isReload, long revNr, char * location, char *inputFile);

time_t bundleArchive_readLastModified(BUNDLE_ARCHIVE archive);
void bundleArchive_writeLastModified(BUNDLE_ARCHIVE archive);

BUNDLE_ARCHIVE bundleArchive_create(char * archiveRoot, long id, char * location, apr_pool_t *mp) {
	BUNDLE_ARCHIVE archive = (BUNDLE_ARCHIVE) malloc(sizeof(*archive));
	archive->id = id;
	archive->location = location;
	archive->archiveRootDir = NULL;
	archive->archiveRoot = archiveRoot;
	archive->revisions = linkedList_create();
	archive->refreshCount = -1;
	time(&archive->lastModified);

	archive->mp = mp;

	bundleArchive_initialize(archive);

	bundleArchive_revise(archive, location, NULL);

	return archive;
}

celix_status_t bundleArchive_destroy(BUNDLE_ARCHIVE archive) {
	if (archive->archiveRootDir != NULL) {
		closedir(archive->archiveRootDir);
	}
	if (archive->archiveRoot != NULL) {
		free(archive->archiveRoot);
	}
	if (archive->revisions != NULL) {
		LINKED_LIST_ITERATOR iter = linkedListIterator_create(archive->revisions, 0);
		while (linkedListIterator_hasNext(iter)) {
			BUNDLE_REVISION revision = linkedListIterator_next(iter);
			bundleRevision_destroy(revision);
		}
		linkedListIterator_destroy(iter);
		linkedList_destroy(archive->revisions);
	}

	free(archive);
	archive = NULL;
	return CELIX_SUCCESS;
}

BUNDLE_ARCHIVE bundleArchive_recreate(char * archiveRoot, apr_pool_t *mp) {
	BUNDLE_ARCHIVE archive = (BUNDLE_ARCHIVE) malloc(sizeof(*archive));
	archive->archiveRoot = archiveRoot;
	archive->archiveRootDir = opendir(archiveRoot);
	archive->id = -1;
	archive->persistentState = -1;
	archive->location = NULL;
	archive->revisions = linkedList_create();
	archive->mp = mp;
	archive->refreshCount = -1;
	archive->lastModified = (time_t) NULL;

	apr_dir_t *dir;
	apr_status_t status = apr_dir_open(&dir, archiveRoot, mp);
	apr_finfo_t dp;
	while ((apr_dir_read(&dp, APR_FINFO_DIRENT|APR_FINFO_TYPE, dir))) {
		if (dp.filetype == APR_DIR && (strncmp(dp.name, "version", 7) == 0)) {
			long idx;
			sscanf(dp.name, "version%*d.%ld", &idx);
		}
	}

	char * location = bundleArchive_getRevisionLocation(archive, 0);
	bundleArchive_revise(archive, location, NULL);

	return archive;
}

long bundleArchive_getId(BUNDLE_ARCHIVE archive) {
	if (archive->id >= 0) {
		return archive->id;
	}

	char bundleId[strlen(archive->archiveRoot) + 11];
	strcpy(bundleId, archive->archiveRoot);
	strcat(bundleId, "/bundle.id");

	apr_file_t *bundleIdFile;
	apr_status_t rv;
	if ((rv = apr_file_open(&bundleIdFile, bundleId, APR_FOPEN_READ, APR_OS_DEFAULT, archive->mp)) != APR_SUCCESS) {

	}
	char id[256];
	apr_file_gets(id, sizeof(id), bundleIdFile);
	apr_file_close(bundleIdFile);

	return atol(id);
}

char * bundleArchive_getLocation(BUNDLE_ARCHIVE archive) {
	if (archive->location != NULL) {
		return archive->location;
	}

	char bundleLocation[strlen(archive->archiveRoot) + 16];
	strcpy(bundleLocation,archive->archiveRoot);
	strcat(bundleLocation, "/bundle.location");
	FILE * bundleLocationFile = fopen(bundleLocation, "r");

	char location[256];
	fgets (location , sizeof(location) , bundleLocationFile);
	fclose(bundleLocationFile);

	return strdup(location);
}

char * bundleArchive_getArchiveRoot(BUNDLE_ARCHIVE archive) {
	return archive->archiveRoot;
}

long bundleArchive_getCurrentRevisionNumber(BUNDLE_ARCHIVE archive) {
	BUNDLE_REVISION revision = bundleArchive_getCurrentRevision(archive);
	return bundleRevision_getNumber(revision);
}

BUNDLE_REVISION bundleArchive_getCurrentRevision(BUNDLE_ARCHIVE archive) {
	return linkedList_isEmpty(archive->revisions) ? NULL : linkedList_getLast(archive->revisions);
}

BUNDLE_REVISION bundleArchive_getRevision(BUNDLE_ARCHIVE archive, long revNr) {
	return linkedList_get(archive->revisions, revNr);
}

BUNDLE_STATE bundleArchive_getPersistentState(BUNDLE_ARCHIVE archive) {
	if (archive->persistentState >= 0) {
		return archive->persistentState;
	}

	char persistentStateLocation[strlen(archive->archiveRoot) + 14];
	strcpy(persistentStateLocation, archive->archiveRoot);
	strcat(persistentStateLocation, "/bundle.state");
	FILE * persistentStateLocationFile = fopen(persistentStateLocation, "r");
	char state[256];
	fgets (state , sizeof(state) , persistentStateLocationFile);
	fclose(persistentStateLocationFile);

	if (state != NULL && (strcmp(state, "active") == 0)) {
		archive->persistentState = BUNDLE_ACTIVE;
	} else if (state != NULL && (strcmp(state, "starting") == 0)) {
		archive->persistentState = BUNDLE_STARTING;
	} else if (state != NULL && (strcmp(state, "uninstalled") == 0)) {
		archive->persistentState = BUNDLE_UNINSTALLED;
	} else {
		archive->persistentState = BUNDLE_INSTALLED;
	}

	return archive->persistentState;
}

void bundleArchive_setPersistentState(BUNDLE_ARCHIVE archive, BUNDLE_STATE state) {
	char persistentStateLocation[strlen(archive->archiveRoot) + 14];
	strcpy(persistentStateLocation, archive->archiveRoot);
	strcat(persistentStateLocation, "/bundle.state");
	FILE * persistentStateLocationFile = fopen(persistentStateLocation, "w");
	char * s;
	switch (state) {
		case BUNDLE_ACTIVE:
			s = "active";
			break;
		case BUNDLE_STARTING:
			s = "starting";
			break;
		case BUNDLE_UNINSTALLED:
			s = "uninstalled";
			break;
		default:
			s = "installed";
			break;
	}
	fprintf(persistentStateLocationFile, "%s", s);
	fclose(persistentStateLocationFile);

	archive->persistentState = state;
}

long bundleArchive_getRefreshCount(BUNDLE_ARCHIVE archive) {
	if (archive->refreshCount >= 0) {
		return archive->refreshCount;
	}

	char refreshCounter[strlen(archive->archiveRoot) + 17];
	strcpy(refreshCounter,archive->archiveRoot);
	strcat(refreshCounter, "/refresh.counter");
	FILE * refreshCounterFile = fopen(refreshCounter, "r");
	if (refreshCounterFile != NULL) {
		char counterStr[256];
		fgets (counterStr , sizeof(counterStr) , refreshCounterFile);
		fclose(refreshCounterFile);
		sscanf(counterStr, "%ld", &archive->refreshCount);
	} else {
		archive->refreshCount = 0;
	}
	return archive->refreshCount;
}

void bundleArchive_setRefreshCount(BUNDLE_ARCHIVE archive) {
	char refreshCounter[strlen(archive->archiveRoot) + 17];
	strcpy(refreshCounter, archive->archiveRoot);
	strcat(refreshCounter, "/refresh.counter");
	FILE * refreshCounterFile = fopen(refreshCounter, "w");

	fprintf(refreshCounterFile, "%ld", archive->refreshCount);
	fclose(refreshCounterFile);
}

time_t bundleArchive_getLastModified(BUNDLE_ARCHIVE archive) {
	if (archive->lastModified == (time_t) NULL) {
		archive->lastModified = bundleArchive_readLastModified(archive);
	}
	return archive->lastModified;
}

void bundleArchive_setLastModified(BUNDLE_ARCHIVE archive, time_t lastModifiedTime) {
	archive->lastModified = lastModifiedTime;
	bundleArchive_writeLastModified(archive);
}

time_t bundleArchive_readLastModified(BUNDLE_ARCHIVE archive) {
	printf("Read time\n");
	char lastModified[strlen(archive->archiveRoot) + 21];
	sprintf(lastModified, "%s/bundle.lastmodified", archive->archiveRoot);

	char timeStr[20];
	apr_file_t *lastModifiedFile;
	apr_status_t status = apr_file_open(&lastModifiedFile, lastModified, APR_FOPEN_READ, APR_OS_DEFAULT, archive->mp);
	apr_file_gets(timeStr, sizeof(timeStr), lastModifiedFile);
	apr_file_close(lastModifiedFile);

	int year, month, day, hours, minutes, seconds;
	sscanf(timeStr, "%d %d %d %d:%d:%d", &year, &month, &day, &hours, &minutes, &seconds);

	struct tm time;
	time.tm_year = year - 1900;
	time.tm_mon = month - 1;
	time.tm_mday = day;
	time.tm_hour = hours;
	time.tm_min = minutes;
	time.tm_sec = seconds;

	return mktime(&time);
}

void bundleArchive_writeLastModified(BUNDLE_ARCHIVE archive) {
	char lastModified[strlen(archive->archiveRoot) + 21];
	sprintf(lastModified, "%s/bundle.lastmodified", archive->archiveRoot);
	char timeStr[20];
	strftime(timeStr, 20, "%Y %m %d %H:%M:%S", localtime(&archive->lastModified));

	apr_file_t *lastModifiedFile;
	apr_status_t status = apr_file_open(&lastModifiedFile, lastModified, APR_FOPEN_CREATE|APR_FOPEN_WRITE, APR_OS_DEFAULT, archive->mp);
	apr_file_printf(lastModifiedFile, "%s", timeStr);
	apr_file_close(lastModifiedFile);
}

void bundleArchive_revise(BUNDLE_ARCHIVE archive, char * location, char *inputFile) {
	long revNr = linkedList_isEmpty(archive->revisions)
			? 0l
			: bundleRevision_getNumber(linkedList_getLast(archive->revisions)) + 1;
	bundleArchive_reviseInternal(archive, false, revNr, location, inputFile);
}

void bundleArchive_reviseInternal(BUNDLE_ARCHIVE archive, bool isReload, long revNr, char * location, char *inputFile) {
	if (inputFile != NULL) {
		location = "inputstream:";
	}

	BUNDLE_REVISION revision = bundleArchive_createRevisionFromLocation(archive, location, inputFile, revNr);

	if (!isReload) {
		bundleArchive_setRevisionLocation(archive, location, revNr);
	}

	linkedList_addElement(archive->revisions, revision);
}

bool bundleArchive_rollbackRevise(BUNDLE_ARCHIVE archive) {
	return true;
}

BUNDLE_REVISION bundleArchive_createRevisionFromLocation(BUNDLE_ARCHIVE archive, char *location, char *inputFile, long revNr) {
	char root[256];
	sprintf(root, "%s/version%ld.%ld", archive->archiveRoot, bundleArchive_getRefreshCount(archive), revNr);

	// TODO create revision using optional FILE *fp;
	BUNDLE_REVISION revision = bundleRevision_create(root, location, revNr, inputFile);
	return revision;
}

char * bundleArchive_getRevisionLocation(BUNDLE_ARCHIVE archive, long revNr) {
	char revisionLocation[256];
	sprintf(revisionLocation, "%s/version%ld.%ld/revision.location", archive->archiveRoot, bundleArchive_getRefreshCount(archive), revNr);

	FILE * revisionLocationFile = fopen(revisionLocation, "r");
	char location[256];
	fgets (location , sizeof(location) , revisionLocationFile);
	fclose(revisionLocationFile);

	return strdup(location);
}

void bundleArchive_setRevisionLocation(BUNDLE_ARCHIVE archive, char * location, long revNr) {
	char revisionLocation[256];
	sprintf(revisionLocation, "%s/version%ld.%ld/revision.location", archive->archiveRoot, bundleArchive_getRefreshCount(archive), revNr);

	FILE * revisionLocationFile = fopen(revisionLocation, "w");
	fprintf(revisionLocationFile, "%s", location);
	fclose(revisionLocationFile);
}

void bundleArchive_close(BUNDLE_ARCHIVE archive) {
	// close revision
	// not yet needed/possible
}

void bundleArchive_closeAndDelete(BUNDLE_ARCHIVE archive) {
	bundleArchive_close(archive);
	bundleArchive_deleteTree(archive->archiveRoot, archive->mp);
}

void bundleArchive_initialize(BUNDLE_ARCHIVE archive) {
	if (archive->archiveRootDir != NULL) {
		return;
	}

	apr_dir_make(archive->archiveRoot, APR_UREAD|APR_UWRITE|APR_UEXECUTE, archive->mp);
	archive->archiveRootDir = opendir(archive->archiveRoot);

	char bundleId[strlen(archive->archiveRoot) + 10];
	strcpy(bundleId, archive->archiveRoot);
	strcat(bundleId, "/bundle.id");
	apr_file_t *bundleIdFile;
	apr_status_t status = apr_file_open(&bundleIdFile, bundleId, APR_FOPEN_CREATE|APR_FOPEN_WRITE, APR_OS_DEFAULT, archive->mp);
	apr_file_printf(bundleIdFile, "%ld", archive->id);
	apr_file_close(bundleIdFile);

//	FILE * bundleIdFile = fopen(bundleId, "w");
//	fprintf(bundleIdFile, "%ld", archive->id);
//	fclose(bundleIdFile);

	char bundleLocation[strlen(archive->archiveRoot) + 16];
	strcpy(bundleLocation,archive->archiveRoot);
	strcat(bundleLocation, "/bundle.location");
	apr_file_t *bundleLocationFile;
	status = apr_file_open(&bundleLocationFile, bundleLocation, APR_FOPEN_CREATE|APR_FOPEN_WRITE, APR_OS_DEFAULT, archive->mp);
	apr_file_printf(bundleLocationFile, "%s", archive->location);
	apr_file_close(bundleLocationFile);

//	FILE * bundleLocationFile = fopen(bundleLocation, "w");
//	fprintf(bundleLocationFile, "%s", archive->location);
//	fclose(bundleLocationFile);

	bundleArchive_writeLastModified(archive);
}

void bundleArchive_deleteTree(char * directory, apr_pool_t *mp) {
	apr_dir_t *dir;
	apr_dir_open(&dir, directory, mp);
	apr_finfo_t dp;
	while (apr_dir_read(&dp, APR_FINFO_DIRENT|APR_FINFO_TYPE, dir)) {

	//DIR * dir = opendir(directory);
//	struct dirent * dp;
//	while ((dp = readdir(dir))) {
		if ((strcmp((dp.name), ".") != 0) && (strcmp((dp.name), "..") != 0)) {
			char subdir[strlen(directory) + strlen(dp.name) + 2];
			strcpy(subdir, directory);
			strcat(subdir, "/");
			strcat(subdir, dp.name);

//			struct stat s;
//			stat(dp->d_name, &s);
			if (dp.filetype == APR_DIR) {
//			if (S_ISDIR(s.st_mode)) {
//			if (dp->d_type == DT_DIR) {
				bundleCache_deleteTree(subdir, mp);
			} else {
				remove(subdir);
			}
		}
	}
	remove(directory);
}
