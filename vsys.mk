local-path := $(call my-dir)

include $(clear-vars)
local.module := flora-cli
local.ndk-script := $(local-path)/ndk-cli.mk
local.ndk-modules := mutils
include $(build-ndk-module)

include $(clear-vars)
local.module := flora-cli.jni
local.ndk-script := $(local-path)/ndk-cli.jni.mk
local.ndk-modules := mutils flora-cli
include $(build-ndk-module)

include $(clear-vars)
local.module := flora-svc
local.ndk-script := $(local-path)/ndk-svc.mk
local.ndk-modules := mutils
include $(build-ndk-module)

include $(clear-vars)
local.module := flora-java
local.src-paths := android/java
include $(build-java-lib)

include $(clear-vars)
local.module := flora-demo
local.src-paths := android/demo
local.manifest := android/demo/AndroidManifest.xml
local.jar-modules := flora-java
local.ndk-modules := flora-cli.jni
include $(build-java-app)
