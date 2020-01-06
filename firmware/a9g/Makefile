
LOCAL_NAME := projects/mailbox

LOCAL_MODULE_DEPENDS := \

LOCAL_API_DEPENDS := \
	libs/gps \
	libs/utils \

LOCAL_ADD_INCLUDE := \
	include \
	include/std_inc \
	include/api_inc \
	libs/cjson/src \
	libs/gps/minmea/src \

IS_ENTRY_POINT := no

MYCFLAGS += 

C_SRC := ${notdir ${wildcard src/*.c}}

include ${SOFT_WORKDIR}/platform/compilation/cust_rules.mk

-include $(patsubst %,${DEPS_REL_PATH}/%.d,$(basename $(C_SRC)))
