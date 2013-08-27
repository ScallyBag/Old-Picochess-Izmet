LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := stockfish
LOCAL_SRC_FILES := \
	benchmark.cpp	book.cpp	evaluate.cpp	misc.cpp	notation.cpp	search.cpp	tt.cpp \
	bitbase.cpp	dgt.cpp		main.cpp	movegen.cpp	pawns.cpp	thread.cpp	uci.cpp \
	bitboard.cpp	endgame.cpp	material.cpp	movepick.cpp	position.cpp	timeman.cpp	ucioption.cpp

LOCAL_CFLAGS    := -I$(ANDROID_NDK)/sources/cxx-stl/stlport/stlport \
		    -mandroid \
		 	-DTARGET_OS=android -D__ANDROID__ \
			-isystem $(ANDROID_ARM_HEADER)/usr/include \
					-DNO_PREFETCH=1
LOCAL_STATIC_LIBRARIES := stlport
LDFLAGS += -static-libstdc++ -static-libgcc


include $(BUILD_EXECUTABLE)

