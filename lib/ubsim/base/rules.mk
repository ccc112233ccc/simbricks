include mk/subdir_pre.mk

lib_ubsim_base := $(d)libubsim_base.a

OBJS := $(addprefix $(d),manager.o if.o)

libubsim_objs += $(OBJS)

$(lib_ubsim_base): $(OBJS)

CLEAN := $(lib_ubsim_base) $(OBJS)
include mk/subdir_post.mk
