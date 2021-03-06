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
 * dm_server.h
 *
 *  \date       15 Oct 2015
 *  \author     <a href="mailto:dev@celix.apache.org">Apache Celix Project Team</a>
 *  \copyright  Apache License, Version 2.0
 */
#ifndef CELIX_DM_INFO_SERVICE_H
#define CELIX_DM_INFO_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include "array_list.h"

#define DM_INFO_SERVICE_NAME "dm_info"


typedef struct dm_interface_info_struct {
    char* name;
    properties_pt properties;
} * dm_interface_info_pt;

typedef struct dm_service_dependency_info_struct {
    char *filter;
    bool available;
    bool required;
    size_t count;
} * dm_service_dependency_info_pt;

typedef struct dm_component_info_struct {
    char id[64];
    char name[128];
    bool active;
    char * state;
    array_list_pt interfaces;   // type dm_interface_info_pt
    array_list_pt dependency_list;  // type dm_service_dependency_info_pt
} * dm_component_info_pt;

typedef struct dm_dependency_manager_info_struct {
    array_list_pt  components;      // type dm_component_info
} * dm_dependency_manager_info_pt;

struct dm_info_service_struct {
    void *handle;

    /*Note: dm_caller has the ownership of the result.*/
    celix_status_t (*getInfo)(void *handle, dm_dependency_manager_info_pt *info);
    void (*destroyInfo)(void *handle, dm_dependency_manager_info_pt info);
};

typedef struct dm_info_service_struct dm_info_service_t;
typedef dm_info_service_t* dm_info_service_pt;

#ifdef __cplusplus
}
#endif

#endif //CELIX_DM_INFO_SERVICE_H
