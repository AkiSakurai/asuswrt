/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <string.h>
#include <jni.h>

#include <android/log.h>
#define LOG_TAG "TR53"
#define LOGI(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__) 

extern int cpe_agent(int argc, char *argv[]);

jstring Java_com_example_TRAgent53_JNI_OneAgentMainLoop(JNIEnv* env,
		jobject thiz) {
	char * argv[] = { "", "-d", "/data/data/com.example.TRAgent53/files/conf" };
	LOGI("This is calling TR53\n");
	cpe_agent(3, argv);
}

