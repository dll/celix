# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

add_subdirectory(publisherA)
add_subdirectory(publisherB)
add_subdirectory(tracker)
add_subdirectory(tracker_depman)

add_deploy("whiteboard" GROUP whiteboard BUNDLES tracker publisherA publisherB shell shell_tui log_service log_writer)
add_deploy("whiteboard_dependency_manager" GROUP whiteboard BUNDLES tracker_depman publisherA publisherB shell shell_tui log_service log_writer dm_shell)