#
# Copyright (C) 2011-2019 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

######## Custom Settings ########

CC 	= gcc-7
CXX = g++-7
Warning_Flags = -Wno-builtin-declaration-mismatch -Wno-literal-suffix -Werror=switch

######## SGX SDK Settings ########

SGX_SDK 	?= /home/love/sgxsdk
SGX_MODE 	?= HW
SGX_ARCH 	?= x64
SGX_DEBUG ?= 1

######## WolfSSL Settings ########

WolfSSL_Library_Path 		= wolfSSL/lib
WolfSSL_Root 						= wolfSSL
WolfSSL_C_Flags 				= -DWOLFSSL_SGX -DWOLFSSL_SHA224 -DWOLFSSL_SHA256 \
	-DWOLFSSL_SHA384 -DWOLFSSL_SHA512 -DWOLFSSL_SHA3 -DWOLFSSL_MD2 \
	-DHAVE_CURVE25519 -DHAVE_SUPPORTED_CURVES -DUSER_TIME
WolfSSL_Enclave_Flags   = -DNO_WOLFSSL_DIR
WolfSSL_Include_Paths		= -I$(WolfSSL_Root)/include
WolfSSL_Link_Flags			= -L$(WolfSSL_Library_Path) -lm -lwolfssl
OpenSSL_Link_Flags 			= -L/usr/local/lib -lssl -lcrypto

ifeq ($(shell getconf LONG_BIT), 32)
	SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CXXFLAGS)), -m32)
	SGX_ARCH := x86
endif

ifeq ($(SGX_ARCH), x86)
	SGX_COMMON_FLAGS := -m32
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r
else
	SGX_COMMON_FLAGS := -m64
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif

ifeq ($(SGX_DEBUG), 1)
ifeq ($(SGX_PRERELEASE), 1)
$(error Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!!)
endif
endif

ifeq ($(SGX_DEBUG), 1)
        SGX_COMMON_FLAGS += -O2 -g
else
        SGX_COMMON_FLAGS += -O2
endif

SGX_COMMON_FLAGS += -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type \
                    -Waddress -Wsequence-point -Wformat-security \
                    -Wmissing-include-dirs -Wfloat-equal -Wundef \
                    -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls
SGX_COMMON_CFLAGS := $(SGX_COMMON_FLAGS) -Wjump-misses-init -Wstrict-prototypes -Wunsuffixed-float-constants
SGX_COMMON_CXXFLAGS := $(SGX_COMMON_FLAGS) -Wnon-virtual-dtor -std=c++17 $(Warning_Flags)

######## Shared Settings ########

Shared_C_Files					:= $(shell find Shared/ -name "*.c")
Shared_Cpp_Files 				:= $(shell find Shared/ -name "*.cpp")
Shared_App_Objects  		:= $(Shared_C_Files:.c=_u.o) $(Shared_Cpp_Files:.cpp=_u.o)
Shared_Enclave_Objects	:= $(Shared_C_Files:.c=_t.o) $(Shared_Cpp_Files:.cpp=_t.o)
Shared_Headers					:= $(shell find Shared/ -name "*.h") $(shell find Shared/ -name "*.hpp")

######## App Settings ########

ifneq ($(SGX_MODE), HW)
	App_Link_Libraries 	:= -lsgx_urts_sim -lsgx_uae_service_sim
else
	App_Link_Libraries  := -lsgx_urts -lsgx_uae_service
endif

App_C_Files 			:= $(shell find App/*/ -name "*.c")
App_Cpp_Files 		:= App/App.cpp $(shell find App/*/ -name "*.cpp")
App_Headers				:= $(shell find App/*/ -name "*.h") $(shell find App/*/ -name "*.hpp")
App_Include_Paths := -IApp -I$(SGX_SDK)/include $(WolfSSL_Include_Paths) -I.

App_C_Flags := -fPIC -Wno-attributes $(App_Include_Paths) $(WolfSSL_C_Flags)

# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(SGX_DEBUG), 1)
        App_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
        App_C_Flags += -DNDEBUG -DEDEBUG -UDEBUG
else
        App_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif

App_Cpp_Flags := $(App_C_Flags)
App_Link_Flags := -L$(SGX_LIBRARY_PATH) $(App_Link_Libraries) -lpthread\
	$(WolfSSL_Link_Flags) $(OpenSSL_Link_Flags) -L/usr/local/lib -lboost_coroutine -lboost_thread

App_Objects := $(App_C_Files:.c=.o) $(App_Cpp_Files:.cpp=.o)

App_Name := app

######## Enclave Settings ########

Enclave_Version_Script := Enclave/Enclave_debug.lds
ifeq ($(SGX_MODE), HW)
ifneq ($(SGX_DEBUG), 1)
ifneq ($(SGX_PRERELEASE), 1)
	# Choose to use 'Enclave.lds' for HW release mode
	Enclave_Version_Script = Enclave/Enclave.lds 
endif
endif
endif

ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif
Crypto_Library_Name := sgx_tcrypto

Enclave_C_Files 			:= $(shell find Enclave/*/ -name "*.c")
Enclave_Cpp_Files 		:= Enclave/Enclave.cpp $(shell find Enclave/*/ -name "*.cpp")
Enclave_Include_Paths := -IEnclave -I$(SGX_SDK)/include -I$(SGX_SDK)/include/libcxx -I$(SGX_SDK)/include/tlibc \
	$(WolfSSL_Include_Paths) -I.
Enclave_Headers				:= $(shell find Enclave/*/ -name "*.h") $(shell find Enclave/*/ -name "*.hpp")

Enclave_C_Flags := -nostdinc -fvisibility=hidden -fpie -fstack-protector $(Enclave_Include_Paths) \
	$(WolfSSL_Enclave_Flags) $(WolfSSL_C_Flags) -DSGX_IN_ENCLAVE
Enclave_Cpp_Flags := $(Enclave_C_Flags) -nostdinc++

# Enable the security flags
Enclave_Security_Link_Flags := -Wl,-z,relro,-z,now,-z,noexecstack

# To generate a proper enclave, it is recommended to follow below guideline to link the trusted libraries:
#    1. Link sgx_trts with the `--whole-archive' and `--no-whole-archive' options,
#       so that the whole content of trts is included in the enclave.
#    2. For other libraries, you just need to pull the required symbols.
#       Use `--start-group' and `--end-group' to link these libraries.
# Do NOT move the libraries linked with `--start-group' and `--end-group' within `--whole-archive' and `--no-whole-archive' options.
# Otherwise, you may get some undesirable errors.
Enclave_Link_Flags := $(Enclave_Security_Link_Flags) \
  -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH) \
	-L$(WolfSSL_Library_Path) -lwolfssl.sgx.static.lib \
	-Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Service_Library_Name) -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0 \
	-Wl,--version-script=$(Enclave_Version_Script)

Enclave_Objects := $(Enclave_C_Files:.c=.o) $(Enclave_Cpp_Files:.cpp=.o)

Enclave_Name := enclave.so
Signed_Enclave_Name := enclave.signed.so
Enclave_Config_File := Enclave/Enclave.config.xml

ifeq ($(SGX_MODE), HW)
ifeq ($(SGX_DEBUG), 1)
	Build_Mode = HW_DEBUG
else ifeq ($(SGX_PRERELEASE), 1)
	Build_Mode = HW_PRERELEASE
else
	Build_Mode = HW_RELEASE
endif
else
ifeq ($(SGX_DEBUG), 1)
	Build_Mode = SIM_DEBUG
else ifeq ($(SGX_PRERELEASE), 1)
	Build_Mode = SIM_PRERELEASE
else
	Build_Mode = SIM_RELEASE
endif
endif


.PHONY: all run target
all: .config_$(Build_Mode)_$(SGX_ARCH)
	@$(MAKE) target

ifeq ($(Build_Mode), HW_RELEASE)
target: $(App_Name) $(Enclave_Name)
	@echo "The project has been built in release hardware mode."
	@echo "Please sign the $(Enclave_Name) first with your signing key before you run the $(App_Name) to launch and access the enclave."
	@echo "To sign the enclave use the command:"
	@echo "   $(SGX_ENCLAVE_SIGNER) sign -key <your key> -enclave $(Enclave_Name) -out <$(Signed_Enclave_Name)> -config $(Enclave_Config_File)"
	@echo "You can also sign the enclave using an external signing tool."
	@echo "To build the project in simulation mode set SGX_MODE=SIM. To build the project in prerelease mode set SGX_PRERELEASE=1 and SGX_MODE=HW."
else
target: $(App_Name) $(Signed_Enclave_Name)
ifeq ($(Build_Mode), HW_DEBUG)
	@echo "The project has been built in debug hardware mode."
else ifeq ($(Build_Mode), SIM_DEBUG)
	@echo "The project has been built in debug simulation mode."
else ifeq ($(Build_Mode), HW_PRERELEASE)
	@echo "The project has been built in pre-release hardware mode."
else ifeq ($(Build_Mode), SIM_PRERELEASE)
	@echo "The project has been built in pre-release simulation mode."
else
	@echo "The project has been built in release simulation mode."
endif
endif

run: all
ifneq ($(Build_Mode), HW_RELEASE)
	@echo "\n//////////////////////\n//  BUILD COMPLETE  //\n//////////////////////\n"
	@$(CURDIR)/$(App_Name)
	@echo "\n//////////////////////\n//   RUN FINISHED   //\n//////////////////////\n"
	@echo "RUN  =>  $(App_Name) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif

.config_$(Build_Mode)_$(SGX_ARCH):
	@rm -f .config_* $(App_Name) $(Enclave_Name) $(Signed_Enclave_Name) \
		$(App_Objects) App/Enclave_u.* $(Enclave_Objects) Enclave/Enclave_t.* $(Shared_App_Objects) $(Shared_Enclave_Objects)
	@touch .config_$(Build_Mode)_$(SGX_ARCH)

######## Shared Objects ########

Shared/%_u.o: Shared/%.c $(Shared_Headers)
	@echo "CC   <=  $<"
	@echo "\033[2m" $(CC) $(SGX_COMMON_CFLAGS) $(App_C_Flags) -c $< -o $@ "\033[0m"
	@$(CC) $(SGX_COMMON_CFLAGS) $(App_C_Flags) -c $< -o $@

Shared/%_u.o: Shared/%.cpp $(Shared_Headers)
	@echo "CXX  <=  $<"
	@echo "\033[2m" $(CXX) $(SGX_COMMON_CXXFLAGS) $(App_Cpp_Flags) -c $< -o $@ "\033[0m"
	@$(CXX) $(SGX_COMMON_CXXFLAGS) $(App_Cpp_Flags) -c $< -o $@

Shared/%_t.o: Shared/%.c $(Shared_Headers)
	@echo "CC   <=  $<"
	@echo "\033[2m" $(CC) $(SGX_COMMON_CFLAGS) $(Enclave_C_Flags) -c $< -o $@ "\033[0m"
	@$(CC) $(SGX_COMMON_CFLAGS) $(Enclave_C_Flags) -c $< -o $@

Shared/%_t.o: Shared/%.cpp $(Shared_Headers)
	@echo "CXX  <=  $<"
	@echo "\033[2m" $(CXX) $(SGX_COMMON_CXXFLAGS) $(Enclave_Cpp_Flags) -c $< -o $@ "\033[0m"
	@$(CXX) $(SGX_COMMON_CXXFLAGS) $(Enclave_Cpp_Flags) -c $< -o $@

######## App Objects ########

App/Enclave_u.h: $(SGX_EDGER8R) $(shell find Enclave/ -name "*.edl")
	@echo "GEN  =>  $@"
	@cd App && $(SGX_EDGER8R) --untrusted ../Enclave/Enclave.edl --search-path ../Enclave --search-path $(SGX_SDK)/include

App/Enclave_u.c: App/Enclave_u.h

App/%.o: App/%.c App/Enclave_u.h $(Shared_Headers) $(App_Headers)
	@echo "CC   <=  $<"
	@echo "\033[2m" $(CC) $(SGX_COMMON_CFLAGS) $(App_C_Flags) -c $< -o $@ "\033[0m"
	@$(CC) $(SGX_COMMON_CFLAGS) $(App_C_Flags) -c $< -o $@

App/%.o: App/%.cpp App/Enclave_u.h $(Shared_Headers) $(App_Headers)
	@echo "CXX  <=  $<"
	@echo "\033[2m" $(CXX) $(SGX_COMMON_CXXFLAGS) $(App_Cpp_Flags) -c $< -o $@ "\033[0m"
	@$(CXX) $(SGX_COMMON_CXXFLAGS) $(App_Cpp_Flags) -c $< -o $@

$(App_Name): App/Enclave_u.o $(App_Objects) $(Shared_App_Objects)
	@echo "LINK =>  $@"
	@echo "\033[2m" $(CXX) $^ -o $@ $(App_Link_Flags) "\033[0m"
	@$(CXX) $^ -o $@ $(App_Link_Flags)

######## Enclave Objects ########

Enclave/Enclave_t.h: $(SGX_EDGER8R) $(shell find Enclave/ -name "*.edl")
	@echo "GEN  =>  $@"
	@cd Enclave && $(SGX_EDGER8R) --trusted ../Enclave/Enclave.edl --search-path ../Enclave --search-path $(SGX_SDK)/include

Enclave/Enclave_t.c: Enclave/Enclave_t.h

Enclave/%.o: Enclave/%.c Enclave/Enclave_t.h $(Shared_Headers) $(Enclave_Headers)
	@echo "CC   <=  $<"
	@echo "\033[2m" @$(CC) $(SGX_COMMON_CFLAGS) $(Enclave_C_Flags) -c $< -o $@ "\033[0m"
	@$(CC) $(SGX_COMMON_CFLAGS) $(Enclave_C_Flags) -c $< -o $@

Enclave/%.o: Enclave/%.cpp Enclave/Enclave_t.h $(Shared_Headers) $(Enclave_Headers)
	@echo "CXX  <=  $<"
	@echo "\033[2m" @$(CXX) $(SGX_COMMON_CXXFLAGS) $(Enclave_Cpp_Flags) -c $< -o $@ "\033[0m"
	@$(CXX) $(SGX_COMMON_CXXFLAGS) $(Enclave_Cpp_Flags) -c $< -o $@

$(Enclave_Objects): Enclave/Enclave_t.h

$(Enclave_Name): Enclave/Enclave_t.o $(Enclave_Objects) $(Shared_Enclave_Objects)
	@echo "LINK =>  $@"
	@echo "\033[2m" $(CXX) $^ -o $@ $(Enclave_Link_Flags) "\033[0m"
	@$(CXX) $^ -o $@ $(Enclave_Link_Flags)

$(Signed_Enclave_Name): $(Enclave_Name)
	@echo "SIGN =>  $@"
	@echo "\033[2m" $(SGX_ENCLAVE_SIGNER) sign -key Enclave/Enclave_private.pem -enclave $(Enclave_Name) -out $@ -config $(Enclave_Config_File) "\033[0m"
	@$(SGX_ENCLAVE_SIGNER) sign -key Enclave/Enclave_private.pem -enclave $(Enclave_Name) -out $@ -config $(Enclave_Config_File)

.PHONY: clean

clean:
	@rm -f .config_* $(App_Name) $(Enclave_Name) $(Signed_Enclave_Name) \
		$(App_Objects) App/Enclave_u.* $(Enclave_Objects) Enclave/Enclave_t.* $(Shared_App_Objects) $(Shared_Enclave_Objects)