include mk/subdir_pre.mk

lib_ubsim := $(lib_dir)libubsim.a

libubsim_objs :=

$(eval $(call subdir,base))
$(eval $(call subdir,mem))

$(lib_ubsim): $(libubsim_objs)
	$(AR) rcs $@ $(libubsim_objs)

CLEAN := $(lib_ubsim)
ALL := $(lib_ubsim)
include mk/subdir_post.mk
