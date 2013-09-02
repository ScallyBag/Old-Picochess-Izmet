LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := stockfish
LOCAL_SRC_FILES := \
	benchmark.cpp	book.cpp	evaluate.cpp	misc.cpp	notation.cpp	search.cpp	tt.cpp \
	bitbase.cpp	dgt.cpp  dgtnix.c		main.cpp	movegen.cpp	pawns.cpp	thread.cpp	uci.cpp \
	bitboard.cpp	endgame.cpp	material.cpp	movepick.cpp	position.cpp	timeman.cpp	ucioption.cpp

LOCAL_CFLAGS    := -I$(ANDROID_NDK)/sources/cxx-stl/stlport/stlport \
		    -mandroid \
		 	-DTARGET_OS=android -D__ANDROID__ \
			-isystem $(ANDROID_NDK)/platforms/android-9/arch-arm/usr/include \
					-DNO_PREFETCH=1 -O3
LOCAL_STATIC_LIBRARIES := stlport
LOCAL_LDLIBS += $(ANDROID_NDK)/sources/cxx-stl/gnu-libstdc++/4.6/libs/armeabi-v7a/libgnustl_static.a
LOCAL_C_INCLUDES := $(ANDROID_NDK)/sources/cxx-stl/gnu-libstdc++/4.6/include 
LOCAL_C_INCLUDES += $(ANDROID_NDK)/sources/cxx-stl/gnu-libstdc++/4.6/libs/armeabi-v7a/include
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := -O3

include $(BUILD_EXECUTABLE)

