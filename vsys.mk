local-path := $(call my-dir)

include $(clear-vars)
local.module := flora
local.ndk-script := $(local-path)/ndk.mk
local.ndk-modules := mutils
include $(build-ndk-module)
