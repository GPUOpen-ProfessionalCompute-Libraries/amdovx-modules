/*
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "loom_shell.h"
#include <stdarg.h>
#if !_WIN32
#include <strings.h>
#define _strnicmp strncasecmp
#define _stricmp  strcasecmp
#endif

#define VERSION          "0.9"
#define SCRIPT_EXTENSION ".lss"
#if _WIN32
#define PROGRAM_NAME     "loom_shell.exe"
#else
#define PROGRAM_NAME     "loom_shell"
#endif

void stitch_log_callback(const char * message)
{
	printf("%s", message);
	fflush(stdout);
}

CLoomShellParser::CLoomShellParser()
{
	// initialize default counts
	num_context_ = DEFAULT_LS_CONTEXT_COUNT;
	num_openvx_context_ = DEFAULT_VX_CONTEXT_COUNT;
	num_opencl_context_ = DEFAULT_CL_CONTEXT_COUNT;
	num_opencl_buf_ = DEFAULT_CL_BUFFER_COUNT;
	// set log callback
	lsGlobalSetLogCallback(stitch_log_callback);
	// create array for contexts, cmd_queues, and buffers
	context_ = new ls_context[num_context_]();
	openvx_context_ = new vx_context[num_openvx_context_]();
	opencl_context_ = new cl_context[num_opencl_context_]();
	opencl_cmd_queue_ = new cl_command_queue[num_opencl_context_]();
	opencl_buf_mem_ = new cl_mem[num_opencl_buf_]();
	opencl_buf_size_ = new vx_uint32[num_opencl_buf_]();
	opencl_buf_cmdq_ = new cl_command_queue[num_opencl_buf_]();
	memset(openvx_context_, 0, sizeof(openvx_context_));
	memset(opencl_context_, 0, sizeof(opencl_context_));
	memset(opencl_cmd_queue_, 0, sizeof(opencl_cmd_queue_));
	memset(opencl_buf_mem_, 0, sizeof(opencl_buf_mem_));
	memset(opencl_buf_size_, 0, sizeof(opencl_buf_size_));
	memset(opencl_buf_cmdq_, 0, sizeof(opencl_buf_cmdq_));
	// misc
	memset(attr_buf_, 0, sizeof(attr_buf_));
}

CLoomShellParser::~CLoomShellParser()
{
	if (context_) {
		for (vx_uint32 i = 0; i < num_context_; i++) {
			if (context_[i]) {
				vx_status status = lsReleaseContext(&context_[i]);
				if (status < 0) Terminate(1, "ERROR: lsReleaseContext(ls[%d]) failed (%d)\n", i, status);
			}
		}
		delete[] context_;
	}
	if (openvx_context_) {
		for (vx_uint32 i = 0; i < num_openvx_context_; i++) {
			if (openvx_context_[i]) {
				vx_status status = vxReleaseContext(&openvx_context_[i]);
				if (status < 0) Terminate(1, "ERROR: vxReleaseContext(vx[%d]) failed (%d)\n", i, status);
			}
		}
		delete[] openvx_context_;
	}
	if (opencl_buf_mem_) {
		for (vx_uint32 i = 0; i < num_opencl_context_; i++) {
			if (opencl_buf_mem_[i]) {
				cl_int status = clReleaseMemObject(opencl_buf_mem_[i]);
				if (status < 0) Terminate(1, "ERROR: clReleaseMemObject(buf[%d]) failed (%d)\n", i, status);
				opencl_buf_mem_[i] = nullptr;
			}
		}
		delete[] opencl_buf_mem_;
	}
	if (opencl_buf_size_) delete[] opencl_buf_size_;
	if (opencl_buf_cmdq_) delete[] opencl_buf_cmdq_;
	if (opencl_cmd_queue_) {
		for (vx_uint32 i = 0; i < num_opencl_context_; i++) {
			if (opencl_cmd_queue_[i]) {
				cl_int status = clReleaseCommandQueue(opencl_cmd_queue_[i]);
				if (status < 0) Terminate(1, "ERROR: clReleaseCommandQueue(cl[%d]) failed (%d)\n", i, status);
				opencl_cmd_queue_[i] = nullptr;
			}
		}
		delete[] opencl_cmd_queue_;
	}
	if (opencl_context_) {
		for (vx_uint32 i = 0; i < num_opencl_context_; i++) {
			if (opencl_context_[i]) {
				cl_int status = clReleaseContext(opencl_context_[i]);
				if (status < 0) Terminate(1, "ERROR: clReleaseContext(cl[%d]) failed (%d)\n", i, status);
				opencl_context_[i] = nullptr;
			}
		}
		delete[] opencl_context_;
	}
}

const char * CLoomShellParser::ParseIndex(const char * s, const char * prefix, vx_uint32& index)
{
	// skip prefix
	if (prefix) {
		s = ParseSkipPattern(s, prefix);
		if (!s) return nullptr;
	}
	// skip initial bracket
	if (*s++ != '[') return nullptr;
	// get index
	s = ParseUInt(s, index);
	if (!s) return nullptr;
	// skip last bracked
	if (*s++ != ']') return nullptr;
	return s;
}

const char * CLoomShellParser::ParseUInt(const char * s, vx_uint32& value)
{
	bool gotValue = false;
	vx_uint32 multiplier = 1;
	while (*s >= '0' && *s <= '9') {
		for (value = 0; *s >= '0' && *s <= '9'; s++) {
			value = value * 10 + (*s - '0');
			gotValue = true;
		}
		value *= multiplier;
		if (*s == '*') {
			// continue processing after multiplication
			s++;
			multiplier = value;
		}
	}
	if (!gotValue) return nullptr;
	return s;
}

const char * CLoomShellParser::ParseInt(const char * s, vx_int32& value)
{
	bool negative = false, gotValue = false;
	if (*s == '-' || *s == '+') {
		negative = (*s == '-') ? true : false;
		s++;
	}
	for (value = 0; *s >= '0' && *s <= '9'; s++) {
		value = value * 10 + (*s - '0');
		gotValue = true;
	}
	if (negative)
		value = -value;
	if (!gotValue) return nullptr;
	return s;
}

const char * CLoomShellParser::ParseFloat(const char * s, vx_float32& value)
{
	bool negative = false, gotValue = false;
	if (*s == '-' || *s == '+') {
		negative = (*s == '-') ? true : false;
		s++;
	}
	for (value = 0.0f; *s >= '0' && *s <= '9'; s++) {
		value = value * 10.0f + (vx_float32)(*s - '0');
		gotValue = true;
	}
	if (*s == '.') {
		vx_float32 f = 1.0f;
		for (s++; *s >= '0' && *s <= '9'; s++) {
			value = value * 10.0f + (vx_float32)(*s - '0');
			f = f * 10.0f;
			gotValue = true;
		}
		value /= f;
	}
	if (negative)
		value = -value;
	if (!gotValue) return nullptr;
	return s;
}

const char * CLoomShellParser::ParseWord(const char * s, char * value, size_t size)
{
	bool gotValue = false;
	// copy word until separator (i.e., SPACE or end paranthesis or end brace)
	for (size_t len = 1; len < size && *s && *s != ' ' && *s != ',' && *s != '{' && *s != '(' && *s != '[' && *s != ']' && *s != ')' && *s != '}'; len++) {
		*value++ = *s++;
		gotValue = true;
	}
	*value = '\0';
	if (!gotValue) return nullptr;
	return s;
}

const char * CLoomShellParser::ParseString(const char * s, char * value, size_t size)
{
	// skip initial quote
	if (*s++ != '"') return nullptr;
	// copy string
	for (size_t len = 1; len < size && *s && *s != '"'; len++)
		*value++ = *s++;
	*value = '\0';
	// skip last quote
	if (*s++ != '"') return nullptr;
	return s;
}

const char * CLoomShellParser::ParseSkipPattern(const char * s, const char * pattern)
{
	// skip whitespace
	while (*s && *s == ' ')
		s++;
	// skip pattern
	for (; *pattern; pattern++, s++) {
		if (*pattern != *s)
			return nullptr;
	}
	// skip whitespace
	while (*s && *s == ' ')
		s++;
	return s;
}

const char * CLoomShellParser::ParseSkip(const char * s, const char * charList)
{
	// skip whitespace
	while (*s && *s == ' ')
		s++;
	// skip pattern
	for (; *charList; charList++) {
		if (*charList != *s)
			return nullptr;
		s++;
		if (*s == ' ') s++;
	}
	// skip whitespace
	while (*s && *s == ' ')
		s++;
	return s;
}

const char * CLoomShellParser::ParseContextWithErrorCheck(const char * s, vx_uint32& index, const char * syntaxError)
{
	s = ParseIndex(s, "ls", index);
	if (!s) {
		Error(syntaxError);
		return nullptr;
	}
	if (index >= num_context_) {
		Error("ERROR: context out-of-range: expects: 0..%d", num_context_ - 1);
		return nullptr;
	}
	if (!context_[index]) {
		Error("ERROR: ls[%d] doesn't exist", index);
		return nullptr;
	}
	return s;
}

int CLoomShellParser::OnCommand()
{
#define SYNTAX_CHECK(call)                  s = call; if(!s) return Error(invalidSyntax);
#define SYNTAX_CHECK_WITH_MESSAGE(call,msg) s = call; if(!s) return Error(msg);
	const char *s = cmd_.c_str();
	// get command and skip whitespace
	char command[64];
	s = ParseWord(s, command, sizeof(command)); if (!s || !command[0]) return Error("ERROR: valid command missing");
	s = ParseSkip(s, "");
	// process command
	if (!_stricmp(command, "lsCreateContext")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsCreateContext() => ls[#]";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		SYNTAX_CHECK(ParseSkipPattern(s, "=>"));
		SYNTAX_CHECK(ParseIndex(s, "ls", contextIndex));
		if (contextIndex >= num_context_) return Error("ERROR: context out-of-range: expects: 0..%d", num_context_ - 1);
		if (context_[contextIndex]) return Error("ERROR: context ls[%d] already exists", contextIndex);
		// process the command
		context_[contextIndex] = lsCreateContext();
		if (!context_[contextIndex]) return Error("ERROR: lsCreateContext() failed");
		Message("..lsCreateContext: created context ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsReleaseContext")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsReleaseContext(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsReleaseContext(&context_[contextIndex]);
		if (status) return Error("ERROR: lsReleaseContext(ls[%d]) failed (%d)", contextIndex, status);
		Message("..lsReleaseContext: released context ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOpenVXContext")) {
		// parse the command
		vx_uint32 contextIndex = 0, vxIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOpenVXContext(ls[#],vx[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseIndex(s, "vx", vxIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (vxIndex >= num_openvx_context_) return Error("ERROR: OpenVX context out-of-range: expects: 0..%d", num_openvx_context_ - 1);
		// process the command
		vx_status status = lsSetOpenVXContext(context_[contextIndex], openvx_context_[vxIndex]);
		if (status) return Error("ERROR: lsSetOpenVXContext(ls[%d],vx[%d]) failed (%d)", contextIndex, vxIndex, status);
		Message("..lsSetOpenVXContext: set OpenVX context vx[%d] for ls[%d]\n", vxIndex, contextIndex);
	}
	else if (!_stricmp(command, "lsSetOpenCLContext")) {
		// parse the command
		vx_uint32 contextIndex = 0, clIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOpenCLContext(ls[#],cl[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseIndex(s, "cl", clIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (clIndex >= num_opencl_context_) return Error("ERROR: OpenCL context out-of-range: expects: 0..%d", num_opencl_context_ - 1);
		// process the command
		vx_status status = lsSetOpenCLContext(context_[contextIndex], opencl_context_[clIndex]);
		if (status) return Error("ERROR: lsSetOpenCLContext(ls[%d],cl[%d]) failed (%d)", contextIndex, clIndex, status);
		Message("..lsSetOpenCLContext: set OpenCL context cl[%d] for ls[%d]\n", clIndex, contextIndex);
	}
	else if (!_stricmp(command, "lsSetRigParams")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		rig_params rig_par = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetRigParams(ls[#],{yaw,pitch,roll,d})";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ",{"), "ERROR: missing '{'");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, rig_par.yaw),"ERROR: invalid yaw value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing pitch value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, rig_par.pitch), "ERROR: invalid pitch value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing roll value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, rig_par.roll), "ERROR: invalid roll value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing d value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, rig_par.d), "ERROR: invalid d value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, "})"), "ERROR: missing '})'");
		// process the command
		vx_status status = lsSetRigParams(context_[contextIndex], &rig_par);
		if (status) return Error("ERROR: lsSetRigParams(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetRigParams: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetCameraConfig")) {
		// parse the command
		vx_uint32 contextIndex = 0, camera_rows = 0, camera_cols = 0, buffer_width = 0, buffer_height = 0; char buffer_format[5] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetCameraConfig(ls[#],rows,cols,format,width,height)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, camera_rows));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, camera_cols));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseWord(s, buffer_format, sizeof(buffer_format)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_width));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_height));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		if (strlen(buffer_format) != 4) return Error("ERROR: buffer_format should have FOUR characters");
		vx_status status = lsSetCameraConfig(context_[contextIndex], camera_rows, camera_cols,
			VX_DF_IMAGE(buffer_format[0], buffer_format[1], buffer_format[2], buffer_format[3]),
			buffer_width, buffer_height);
		if (status) return Error("ERROR: lsSetCameraConfig(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetCameraConfig: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOutputConfig")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_width = 0, buffer_height = 0; char buffer_format[5] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOutputConfig(ls[#],format,width,height)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseWord(s, buffer_format, sizeof(buffer_format)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_width));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_height));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		if (strlen(buffer_format) != 4) return Error("ERROR: buffer_format should have FOUR characters");
		vx_status status = lsSetOutputConfig(context_[contextIndex],
			VX_DF_IMAGE(buffer_format[0], buffer_format[1], buffer_format[2], buffer_format[3]),
			buffer_width, buffer_height);
		if (status) return Error("ERROR: lsSetOutputConfig(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetOutputConfig: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOverlayConfig")) {
		// parse the command
		vx_uint32 contextIndex = 0, overlay_rows = 0, overlay_cols = 0, buffer_width = 0, buffer_height = 0; char buffer_format[5] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOverlayConfig(ls[#],rows,cols,format,width,height)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, overlay_rows));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, overlay_cols));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseWord(s, buffer_format, sizeof(buffer_format)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_width));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_height));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		if (strlen(buffer_format) != 4) return Error("ERROR: buffer_format should have FOUR characters");
		vx_status status = lsSetOverlayConfig(context_[contextIndex], overlay_rows, overlay_cols,
			VX_DF_IMAGE(buffer_format[0], buffer_format[1], buffer_format[2], buffer_format[3]),
			buffer_width, buffer_height);
		if (status) return Error("ERROR: lsSetOverlayConfig(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetOverlayConfig: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetCameraParams") || !_stricmp(command, "lsSetOverlayParams")) {
		bool isCamera = !_stricmp(command, "lsSetCameraParams") ? true : false;
		// parse the command
		vx_uint32 contextIndex = 0, index = 0; vx_int32 lens_type = 0;
		camera_params camera_par = { 0 };
		const char * invalidSyntax = isCamera ?
			"ERROR: invalid syntax: expects: lsSetCameraParams(ls[#],index,{{yaw,pitch,roll,tx,ty,tz},{lens,haw,hfov,k1,k2,k3,du0,dv0,rcrop}})" :
			"ERROR: invalid syntax: expects: lsSetOverlayParams(ls[#],index,{{yaw,pitch,roll,tx,ty,tz},{lens,haw,hfov,k1,k2,k3,du0,dv0,rcrop}})";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, index));
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ",{{"), "ERROR: missing pitch value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.focal.yaw),"ERROR: invalid yaw value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing pitch value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.focal.pitch), "ERROR: invalid pitch value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing roll value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.focal.roll), "ERROR: invalid roll value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing tx value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.focal.tx), "ERROR: invalid tx value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing ty value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.focal.ty), "ERROR: invalid ty value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing tz value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.focal.tz), "ERROR: invalid tz value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, "},{"), "ERROR: missing separator before lens_type");
		SYNTAX_CHECK_WITH_MESSAGE(ParseInt(s, lens_type), "ERROR: invalid lens_type value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing haw value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.haw), "ERROR: invalid haw value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing hfov value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.hfov), "ERROR: invalid hfov value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing k1 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.k1), "ERROR: invalid k1 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing k2 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.k2), "ERROR: invalid k2 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing k3 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.k3), "ERROR: invalid k3 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing du0 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.du0), "ERROR: invalid du0 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing dv0 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.dv0), "ERROR: invalid dv0 value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, ","), "ERROR: missing r_crop value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseFloat(s, camera_par.lens.r_crop), "ERROR: invalid r_crop value");
		SYNTAX_CHECK_WITH_MESSAGE(ParseSkip(s, "}})"), "ERROR: missing '}})'");
		camera_par.lens.lens_type = (camera_lens_type)lens_type;
		if (Verbose()) {
			Message("..%s: index#%d {{%8.3f,%8.3f,%8.3f,%.0f,%.0f,%.0f},{%d,%.0f,%.3f,%f,%f,%f,%.3f,%.3f,%.1f}}\n", command, index,
				camera_par.focal.yaw, camera_par.focal.pitch, camera_par.focal.roll,
				camera_par.focal.tx, camera_par.focal.ty, camera_par.focal.tz,
				camera_par.lens.lens_type, camera_par.lens.haw, camera_par.lens.hfov,
				camera_par.lens.k1, camera_par.lens.k2, camera_par.lens.k3,
				camera_par.lens.du0, camera_par.lens.dv0, camera_par.lens.r_crop);
		}
		// process the command
		if (isCamera) {
			vx_status status = lsSetCameraParams(context_[contextIndex], index, &camera_par);
			if (status) return Error("ERROR: lsSetCameraParams(ls[%d],%d,*) failed (%d)", contextIndex, index, status);
			Message("..lsSetCameraParams: successful for ls[%d] and camera#%d\n", contextIndex, index);
		}
		else {
			vx_status status = lsSetOverlayParams(context_[contextIndex], index, &camera_par);
			if (status) return Error("ERROR: lsSetOverlayParams(ls[%d],%d,*) failed (%d)", contextIndex, index, status);
			Message("..lsSetOverlayParams: successful for ls[%d] and overlay#%d\n", contextIndex, index);
		}
	}
	else if (!_stricmp(command, "lsSetCameraBufferStride")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_stride_in_bytes = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetCameraBufferStride(ls[#],stride_in_bytes)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_stride_in_bytes));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetCameraBufferStride(context_[contextIndex], buffer_stride_in_bytes);
		if (status) return Error("ERROR: lsSetCameraBufferStride(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetCameraBufferStride: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOutputBufferStride")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_stride_in_bytes = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOutputBufferStride(ls[#],stride_in_bytes)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_stride_in_bytes));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetOutputBufferStride(context_[contextIndex], buffer_stride_in_bytes);
		if (status) return Error("ERROR: lsSetOutputBufferStride(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetOutputBufferStride: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOverlayBufferStride")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_stride_in_bytes = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOverlayBufferStride(ls[#],stride_in_bytes)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, buffer_stride_in_bytes));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetOverlayBufferStride(context_[contextIndex], buffer_stride_in_bytes);
		if (status) return Error("ERROR: lsSetOverlayBufferStride(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetOverlayBufferStride: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetCameraModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetCameraModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, module, sizeof(module)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelName, sizeof(kernelName)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelArguments, sizeof(kernelArguments)));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetCameraModule(context_[contextIndex], module, kernelName, kernelArguments);
		if (status) return Error("ERROR: lsSetCameraModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetCameraModule: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOutputModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOutputModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, module, sizeof(module)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelName, sizeof(kernelName)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelArguments, sizeof(kernelArguments)));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetOutputModule(context_[contextIndex], module, kernelName, kernelArguments);
		if (status) return Error("ERROR: lsSetOutputModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetOutputModule: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetOverlayModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOverlayModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, module, sizeof(module)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelName, sizeof(kernelName)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelArguments, sizeof(kernelArguments)));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetOverlayModule(context_[contextIndex], module, kernelName, kernelArguments);
		if (status) return Error("ERROR: lsSetOverlayModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetOverlayModule: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetViewingModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetViewingModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, module, sizeof(module)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelName, sizeof(kernelName)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, kernelArguments, sizeof(kernelArguments)));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsSetViewingModule(context_[contextIndex], module, kernelName, kernelArguments);
		if (status) return Error("ERROR: lsSetViewingModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsSetViewingModule: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsInitialize")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsInitialize(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsInitialize(context_[contextIndex]);
		if (status) return Error("ERROR: lsInitialize(ls[%d]) failed (%d)", contextIndex, status);
		Message("..lsInitialize: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsReinitialize")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsReinitialize(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsReinitialize(context_[contextIndex]);
		if (status) return Error("ERROR: lsReinitialize(ls[%d]) failed (%d)", contextIndex, status);
		Message("..lsReinitialize: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsScheduleFrame")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsScheduleFrame(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsScheduleFrame(context_[contextIndex]);
		if (status) return Error("ERROR: lsScheduleFrame(ls[%d]) failed (%d)", contextIndex, status);
		Message("..lsScheduleFrame: successful for ls[%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsWaitForCompletion")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsWaitForCompletion(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsWaitForCompletion(context_[contextIndex]);
		if (status) return Error("ERROR: lsWaitForCompletion(ls[%d]) failed (%d)", contextIndex, status);
		Message("..lsWaitForCompletion: successful for [%d]\n", contextIndex);
	}
	else if (!_stricmp(command, "lsSetCameraBuffer")) {
		// parse the command
		vx_uint32 contextIndex = 0, bufIndex = 0;
		bool useNull = false;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetCameraBuffer(ls[#],buf[#]|NULL)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		if (!_stricmp(s, ",null)")) {
			useNull = true;
		}
		else {
			SYNTAX_CHECK(ParseSkip(s, ","));
			SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
			SYNTAX_CHECK(ParseSkip(s, ")"));
			if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		}
		// process the command
		if (useNull) {
			vx_status status = lsSetCameraBuffer(context_[contextIndex], nullptr);
			if (status) return Error("ERROR: lsSetCameraBuffer(ls[%d],NULL) failed (%d)", contextIndex, status);
			Message("..lsSetCameraBuffer: set NULL for ls[%d]\n", contextIndex);
		}
		else {
			vx_status status = lsSetCameraBuffer(context_[contextIndex], &opencl_buf_mem_[bufIndex]);
			if (status) return Error("ERROR: lsSetCameraBuffer(ls[%d],buf[%d]) failed (%d)", contextIndex, bufIndex, status);
			Message("..lsSetCameraBuffer: set OpenCL buffer buf[%d] for ls[%d]\n", bufIndex, contextIndex);
		}
	}
	else if (!_stricmp(command, "lsSetOutputBuffer")) {
		// parse the command
		vx_uint32 contextIndex = 0, bufIndex = 0;
		bool useNull = false;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOutputBuffer(ls[#],buf[#]|NULL)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		if (!_stricmp(s, ",null)")) {
			useNull = true;
		}
		else {
			SYNTAX_CHECK(ParseSkip(s, ","));
			SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
			SYNTAX_CHECK(ParseSkip(s, ")"));
			if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		}
		// process the command
		if (useNull) {
			vx_status status = lsSetOutputBuffer(context_[contextIndex], nullptr);
			if (status) return Error("ERROR: lsSetOutputBuffer(ls[%d],NULL) failed (%d)", contextIndex, status);
			Message("..lsSetOutputBuffer: set NULL for ls[%d]\n", contextIndex);
		}
		else {
			vx_status status = lsSetOutputBuffer(context_[contextIndex], &opencl_buf_mem_[bufIndex]);
			if (status) return Error("ERROR: lsSetOutputBuffer(ls[%d],buf[%d]) failed (%d)", contextIndex, bufIndex, status);
			Message("..lsSetOutputBuffer: set OpenCL buffer buf[%d] for ls[%d]\n", bufIndex, contextIndex);
		}
	}
	else if (!_stricmp(command, "lsSetOverlayBuffer")) {
		// parse the command
		vx_uint32 contextIndex = 0, bufIndex = 0;
		bool useNull = false;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetOverlayBuffer(ls[#],buf[#]|NULL)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		if (!_stricmp(s, ",null)")) {
			useNull = true;
		}
		else {
			SYNTAX_CHECK(ParseSkip(s, ","));
			SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
			SYNTAX_CHECK(ParseSkip(s, ")"));
			if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		}
		// process the command
		if (useNull) {
			vx_status status = lsSetOverlayBuffer(context_[contextIndex], nullptr);
			if (status) return Error("ERROR: lsSetOverlayBuffer(ls[%d],NULL) failed (%d)", contextIndex, status);
			Message("..lsSetOverlayBuffer: set NULL for ls[%d]\n", contextIndex);
		}
		else {
			vx_status status = lsSetOverlayBuffer(context_[contextIndex], &opencl_buf_mem_[bufIndex]);
			if (status) return Error("ERROR: lsSetOverlayBuffer(ls[%d],buf[%d]) failed (%d)", contextIndex, bufIndex, status);
			Message("..lsSetOverlayBuffer: set OpenCL buffer buf[%d] for ls[%d]\n", bufIndex, contextIndex);
		}
	}
	else if (!_stricmp(command, "lsSetAttributes")) {
		// parse the command
		vx_uint32 contextIndex = 0, attr_offset, attr_count = 0;
		char fileName[256] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsSetAttributes(ls[#],offset,count,{value(s)}|\"attr.txt\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, attr_offset));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, attr_count));
		if (attr_offset >= LIVE_STITCH_ATTR_MAX_COUNT || (attr_offset + attr_count) >= LIVE_STITCH_ATTR_MAX_COUNT) return Error("ERROR: invalid attr_offset and/or attr_count");
		SYNTAX_CHECK(ParseSkip(s, ","));
		if (*s == '"') {
			SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
			FILE * fp = fopen(fileName, "r"); if (!fp) return Error("ERROR: unable to open: %s", fileName);
			for (vx_uint32 i = 0; i < attr_count; i++) {
				if (fscanf(fp, "%f", &attr_buf_[attr_offset + i]) != 1) return Error("ERROR: missing attributes in: %s", fileName);
			}
			fclose(fp);
		}
		else {
			for (vx_uint32 i = 0; i < attr_count; i++) {
				SYNTAX_CHECK(ParseSkip(s, (i > 0) ? "," : "{"));
				SYNTAX_CHECK(ParseFloat(s, attr_buf_[attr_offset + i]));
			}
			SYNTAX_CHECK(ParseSkip(s, "}"));
		}
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		FILE * fp = fopen(fileName, "r"); if (!fp) return Error("ERROR: unable to open: %s", fileName);
		for (vx_uint32 i = 0; i < attr_count; i++) {
			if (fscanf(fp, "%f", &attr_buf_[attr_offset + i]) != 1) return Error("ERROR: missing attributes in: %s", fileName);
		}
		fclose(fp);
		vx_status status = lsSetAttributes(context_[contextIndex], attr_offset, attr_count, &attr_buf_[attr_offset]);
		if (status) return Error("ERROR: lsSetAttributes(ls[%d],%d,%d,*) failed (%d)", contextIndex, attr_offset, attr_count, status);
		Message("..lsSetAttributes: %d..%d (count %d) loaded to ls[%d] from \"%s\"\n", attr_offset, attr_offset + attr_count - 1, attr_count, contextIndex, fileName[0] ? fileName : "command");
	}
	else if (!_stricmp(command, "lsGetAttributes")) {
		// parse the command
		vx_uint32 contextIndex = 0, attr_offset, attr_count = 0;
		char fileName[256] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetAttributes(ls[#],offset,count,\"attr.txt\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, attr_offset));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, attr_count));
		if (attr_offset >= LIVE_STITCH_ATTR_MAX_COUNT || (attr_offset + attr_count) >= LIVE_STITCH_ATTR_MAX_COUNT) return Error("ERROR: invalid attr_offset and/or attr_count");
		SYNTAX_CHECK(ParseSkip(s, ""));
		if (*s == ',') {
			SYNTAX_CHECK(ParseSkip(s, ","));
			SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
		}
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetAttributes(context_[contextIndex], attr_offset, attr_count, &attr_buf_[attr_offset]);
		if (status) return Error("ERROR: lsGetAttributes(ls[%d],%d,%d,*) failed (%d)", contextIndex, attr_offset, attr_count, status);
		FILE * fp = stdout;
		if (strlen(fileName) > 0) {
			fp = fopen(fileName, "w");
			if (!fp) return Error("ERROR: unable to create: %s", fileName);
		}
		for (vx_uint32 i = 0; i < attr_count; i++) {
			fprintf(fp, "%13.6f\n", attr_buf_[attr_offset + i]);
		}
		fflush(fp);
		if(fp != stdout)fclose(fp);
		Message("..lsGetAttributes: %d..%d (count %d) saved from ls[%d] to \"%s\"\n", attr_offset, attr_offset + attr_count - 1, attr_count, contextIndex, fileName[0] ? fileName : "console");
	}
	else if (!_stricmp(command, "lsGlobalSetAttributes")) {
		// parse the command
		vx_uint32 attr_offset, attr_count = 0;
		char fileName[256] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGlobalSetAttributes(offset,count,{value(s)}|\"attr.txt\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseUInt(s, attr_offset));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, attr_count));
		if (attr_offset >= LIVE_STITCH_ATTR_MAX_COUNT || (attr_offset + attr_count) >= LIVE_STITCH_ATTR_MAX_COUNT) return Error("ERROR: invalid attr_offset and/or attr_count");
		SYNTAX_CHECK(ParseSkip(s, ","));
		if (*s == '"') {
			SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
			FILE * fp = fopen(fileName, "r"); if (!fp) return Error("ERROR: unable to open: %s", fileName);
			for (vx_uint32 i = 0; i < attr_count; i++) {
				if (fscanf(fp, "%f", &attr_buf_[attr_offset + i]) != 1) return Error("ERROR: missing attributes in: %s", fileName);
			}
			fclose(fp);
		}
		else {
			for (vx_uint32 i = 0; i < attr_count; i++) {
				SYNTAX_CHECK(ParseSkip(s, (i > 0) ? "," : "{"));
				SYNTAX_CHECK(ParseFloat(s, attr_buf_[attr_offset + i]));
			}
			SYNTAX_CHECK(ParseSkip(s, "}"));
		}
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGlobalSetAttributes(attr_offset, attr_count, &attr_buf_[attr_offset]);
		if (status) return Error("ERROR: lsGlobalSetAttributes(%d,%d,*) failed (%d)", attr_offset, attr_count, status);
		Message("..lsGlobalSetAttributes: %d..%d (count %d) loaded from \"%s\"\n", attr_offset, attr_offset + attr_count - 1, attr_count, fileName[0] ? fileName : "command");
	}
	else if (!_stricmp(command, "lsGlobalGetAttributes")) {
		// parse the command
		vx_uint32 attr_offset, attr_count = 0;
		char fileName[256] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGlobalGetAttributes(offset,count,\"attr.txt\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseUInt(s, attr_offset));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, attr_count));
		if (attr_offset >= LIVE_STITCH_ATTR_MAX_COUNT || (attr_offset + attr_count) >= LIVE_STITCH_ATTR_MAX_COUNT) return Error("ERROR: invalid attr_offset and/or attr_count");
		SYNTAX_CHECK(ParseSkip(s, ""));
		if (*s == ',') {
			SYNTAX_CHECK(ParseSkip(s, ","));
			SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
		}
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGlobalGetAttributes(attr_offset, attr_count, &attr_buf_[attr_offset]);
		if (status) return Error("ERROR: lsGlobalGetAttributes(%d,%d,*) failed (%d)", attr_offset, attr_count, status);
		FILE * fp = stdout;
		if (strlen(fileName) > 0) {
			fp = fopen(fileName, "w");
			if (!fp) return Error("ERROR: unable to create: %s", fileName);
		}
		for (vx_uint32 i = 0; i < attr_count; i++) {
			fprintf(fp, "%13.6f\n", attr_buf_[attr_offset + i]);
		}
		fflush(fp);
		if (fp != stdout)fclose(fp);
		Message("..lsGlobalGetAttributes: %d..%d (count %d) saved to \"%s\"\n", attr_offset, attr_offset + attr_count - 1, attr_count, fileName[0] ? fileName : "console");
	}
	else if (!_stricmp(command, "lsGetOpenVXContext")) {
		// parse the command
		vx_uint32 contextIndex = 0, vxIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOpenVXContext(ls[#],vx[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseIndex(s, "vx", vxIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (vxIndex >= num_openvx_context_) return Error("ERROR: OpenVX context out-of-range: expects: 0..%d", num_openvx_context_ - 1);
		if (openvx_context_[vxIndex]) return Error("ERROR: OpenVX context vx[%d] already exists\n", vxIndex);
		// process the command
		vx_status status = lsGetOpenVXContext(context_[contextIndex], &openvx_context_[vxIndex]);
		if (status) return Error("ERROR: lsGetOpenVXContext(ls[%d],vx[%d]) failed (%d)", contextIndex, vxIndex, status);
		Message("..lsGetOpenVXContext: get OpenVX context vx[%d] from ls[%d]\n", vxIndex, contextIndex);
	}
	else if (!_stricmp(command, "lsGetOpenCLContext")) {
		// parse the command
		vx_uint32 contextIndex = 0, clIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOpenCLContext(ls[#],cl[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseIndex(s, "cl", clIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (clIndex >= num_opencl_context_) return Error("ERROR: OpenCL context out-of-range: expects: 0..%d", num_opencl_context_ - 1);
		if (opencl_context_[clIndex]) return Error("ERROR: OpenCL context cl[%d] already exists\n", clIndex);
		// process the command
		vx_status status = lsGetOpenCLContext(context_[contextIndex], &opencl_context_[clIndex]);
		if (status) return Error("ERROR: lsGetOpenCLContext(ls[%d],cl[%d]) failed (%d)", contextIndex, clIndex, status);
		Message("..lsGetOpenCLContext: get OpenCL context cl[%d] from ls[%d]\n", clIndex, contextIndex);
		// create OpenCL cmd_queue
		cl_int err; cl_device_id device_id = nullptr;
		err = clGetContextInfo(opencl_context_[clIndex], CL_CONTEXT_DEVICES, sizeof(device_id), &device_id, nullptr);
		if (err) return Error("ERROR: clGetContextInfo(*,CL_CONTEXT_DEVICES) failed (%d)", err);
		opencl_cmd_queue_[clIndex] = clCreateCommandQueueWithProperties(opencl_context_[clIndex], device_id, 0, &err);
		if (!opencl_cmd_queue_[clIndex]) return Error("ERROR: clCreateCommandQueueWithProperties: failed (%d)", err);
		Message("..got OpenCL context from ls[%d] and created cmd_queue for OpenCL cl[%d]\n", contextIndex, clIndex);
	}
	else if (!_stricmp(command, "lsGetRigParams")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		rig_params rig_par = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetRigParams(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetRigParams(context_[contextIndex], &rig_par);
		if (status) return Error("ERROR: lsGetRigParams(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetRigParams: ls[%d]: {%.3f,%.3f,%.3f,%.3f}\n", contextIndex, rig_par.yaw, rig_par.pitch, rig_par.roll, rig_par.d);
	}
	else if (!_stricmp(command, "lsGetCameraConfig")) {
		// parse the command
		vx_uint32 contextIndex = 0, camera_rows = 0, camera_cols = 0, buffer_width = 0, buffer_height = 0; char buffer_format[5] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetCameraConfig(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetCameraConfig(context_[contextIndex], &camera_rows, &camera_cols,
			(vx_df_image *)&buffer_format, &buffer_width, &buffer_height);
		if (status) return Error("ERROR: lsGetCameraConfig(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetCameraConfig: ls[%d]: %dx%d %4.4s %dx%d\n", contextIndex, camera_rows, camera_cols, &buffer_format, buffer_width, buffer_height);
	}
	else if (!_stricmp(command, "lsGetOutputConfig")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_width = 0, buffer_height = 0; char buffer_format[5] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOutputConfig(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetOutputConfig(context_[contextIndex], (vx_df_image *)&buffer_format, &buffer_width, &buffer_height);
		if (status) return Error("ERROR: lsGetOutputConfig(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetOutputConfig: ls[%d]: %4.4s %dx%d\n", contextIndex, &buffer_format, buffer_width, buffer_height);
	}
	else if (!_stricmp(command, "lsGetOverlayConfig")) {
		// parse the command
		vx_uint32 contextIndex = 0, overlay_rows = 0, overlay_cols = 0, buffer_width = 0, buffer_height = 0; char buffer_format[5] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOverlayConfig(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetOverlayConfig(context_[contextIndex], &overlay_rows, &overlay_cols,
			(vx_df_image *)&buffer_format, &buffer_width, &buffer_height);
		if (status) return Error("ERROR: lsGetOverlayConfig(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetOverlayConfig: ls[%d]: %dx%d %4.4s %dx%d\n", contextIndex, overlay_rows, overlay_cols, &buffer_format, buffer_width, buffer_height);
	}
	else if (!_stricmp(command, "lsGetCameraParams") || !_stricmp(command, "lsGetOverlayParams")) {
		bool isCamera = !_stricmp(command, "lsGetCameraParams") ? true : false;
		// parse the command
		vx_uint32 contextIndex = 0, index = 0;
		camera_params camera_par = { 0 };
		const char * invalidSyntax = isCamera ?
			"ERROR: invalid syntax: expects: lsGetCameraParams(ls[#],index)" :
			"ERROR: invalid syntax: expects: lsGetOverlayParams(ls[#],index)";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, index));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		if (isCamera) {
			vx_status status = lsGetCameraParams(context_[contextIndex], index, &camera_par);
			if (status) return Error("ERROR: lsGetCameraParams(ls[%d],%d,*) failed (%d)", contextIndex, index, status);
		}
		else {
			vx_status status = lsGetOverlayParams(context_[contextIndex], index, &camera_par);
			if (status) return Error("ERROR: lsSetOverlayParams(ls[%d],%d,*) failed (%d)", contextIndex, index, status);
		}
		Message("..%s: index#%d {{%8.3f,%8.3f,%8.3f,%.0f,%.0f,%.0f},{%d,%.0f,%.3f,%f,%f,%f,%.3f,%.3f,%.1f}}\n", command, index,
			camera_par.focal.yaw, camera_par.focal.pitch, camera_par.focal.roll,
			camera_par.focal.tx, camera_par.focal.ty, camera_par.focal.tz,
			camera_par.lens.lens_type, camera_par.lens.haw, camera_par.lens.hfov,
			camera_par.lens.k1, camera_par.lens.k2, camera_par.lens.k3,
			camera_par.lens.du0, camera_par.lens.dv0, camera_par.lens.r_crop);
	}
	else if (!_stricmp(command, "lsGetCameraBufferStride")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_stride_in_bytes = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetCameraBufferStride(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetCameraBufferStride(context_[contextIndex], &buffer_stride_in_bytes);
		if (status) return Error("ERROR: lsGetCameraBufferStride(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetCameraBufferStride: ls[%d] buffer_stride_in_bytes: %d\n", contextIndex, buffer_stride_in_bytes);
	}
	else if (!_stricmp(command, "lsGetOutputBufferStride")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_stride_in_bytes = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOutputBufferStride(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetOutputBufferStride(context_[contextIndex], &buffer_stride_in_bytes);
		if (status) return Error("ERROR: lsGetOutputBufferStride(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetOutputBufferStride: ls[%d] buffer_stride_in_bytes: %d\n", contextIndex, buffer_stride_in_bytes);
	}
	else if (!_stricmp(command, "lsGetOverlayBufferStride")) {
		// parse the command
		vx_uint32 contextIndex = 0, buffer_stride_in_bytes = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOverlayBufferStride(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetOverlayBufferStride(context_[contextIndex], &buffer_stride_in_bytes);
		if (status) return Error("ERROR: lsGetOverlayBufferStride(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetOverlayBufferStride: ls[%d] buffer_stride_in_bytes: %d\n", contextIndex, buffer_stride_in_bytes);
	}
	else if (!_stricmp(command, "lsGetCameraModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetCameraModule(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetCameraModule(context_[contextIndex], module, sizeof(module), kernelName, sizeof(kernelName), kernelArguments, sizeof(kernelArguments));
		if (status) return Error("ERROR: lsGetCameraModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetCameraModule: ls[%d]: \"%s\" \"%s\" \"%s\"\n", contextIndex, module, kernelName, kernelArguments);
	}
	else if (!_stricmp(command, "lsGetOutputModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOutputModule(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetOutputModule(context_[contextIndex], module, sizeof(module), kernelName, sizeof(kernelName), kernelArguments, sizeof(kernelArguments));
		if (status) return Error("ERROR: lsGetOutputModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetOutputModule: ls[%d]: \"%s\" \"%s\" \"%s\"\n", contextIndex, module, kernelName, kernelArguments);
	}
	else if (!_stricmp(command, "lsGetOverlayModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetOverlayModule(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetOverlayModule(context_[contextIndex], module, sizeof(module), kernelName, sizeof(kernelName), kernelArguments, sizeof(kernelArguments));
		if (status) return Error("ERROR: lsGetOverlayModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetOverlayModule: ls[%d]: \"%s\" \"%s\" \"%s\"\n", contextIndex, module, kernelName, kernelArguments);
	}
	else if (!_stricmp(command, "lsGetViewingModule")) {
		// parse the command
		vx_uint32 contextIndex = 0;
		char module[256], kernelName[64], kernelArguments[1024];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsGetViewingModule(ls[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		vx_status status = lsGetViewingModule(context_[contextIndex], module, sizeof(module), kernelName, sizeof(kernelName), kernelArguments, sizeof(kernelArguments));
		if (status) return Error("ERROR: lsGetViewingModule(ls[%d],*) failed (%d)", contextIndex, status);
		Message("..lsGetViewingModule: ls[%d]: \"%s\" \"%s\" \"%s\"\n", contextIndex, module, kernelName, kernelArguments);
	}
	else if (!_stricmp(command, "lsExportConfiguration")) {
		// parse the command
		vx_uint32 contextIndex = 0; char exportType[128], fileName[256] = { 0 };
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsExportConfiguration(ls[#],\"<exportType>\",\"<fileName>\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, exportType, sizeof(exportType)));
		SYNTAX_CHECK(ParseSkip(s, ""));
		if (*s == ',') {
			SYNTAX_CHECK(ParseSkip(s, ","));
			SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
		}
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		char text[8192];
		vx_status status = lsExportConfiguration(context_[contextIndex], exportType, text, sizeof(text));
		if (status) return Error("ERROR: lsExportConfiguration(ls[%d],\"%s\",...) failed (%d)", contextIndex, exportType);
		if (!_stricmp(fileName, "")) {
			Message("..lsExportConfiguration: ls[%d] %s: begin\n", contextIndex, exportType);
			Message("%s", text);
			Message("..lsExportConfiguration: ls[%d] %s: end\n", contextIndex, exportType);
		}
		else {
			FILE * fp = fopen(fileName, "w");
			if (!fp) return Error("ERROR: unable to create: %s\n", fileName);
			fprintf(fp, "%s", text);
			fclose(fp);
			Message("..lsExportConfiguration: ls[%d] %s: created %s\n", contextIndex, exportType, fileName);
		}
	}
	else if (!_stricmp(command, "lsImportConfiguration")) {
		// parse the command
		vx_uint32 contextIndex = 0; char importType[128], fileName[256];
		const char * invalidSyntax = "ERROR: invalid syntax: expects: lsImportConfiguration(ls[#],\"<importType>\",\"<fileName>\")";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, importType, sizeof(importType)));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		// process the command
		FILE * fp = fopen(fileName, "rb");
		if (!fp) return Error("ERROR: unable to open: %s\n", fileName);
		fseek(fp, 0L, SEEK_END); size_t size = ftell(fp); fseek(fp, 0L, SEEK_SET);
		char * text = new char[size + 1]; text[size] = '\0';
		fread(text, 1, size, fp);
		fclose(fp);
		vx_status status = lsImportConfiguration(context_[contextIndex], importType, text);
		delete[] text;
		if (status) return Error("ERROR: lsImportConfiguration(ls[%d],\"%s\",...) failed (%d)", contextIndex, importType);
		Message("..lsImportConfiguration: ls[%d] %s: imported %s\n", contextIndex, importType, fileName);
	}
	else if (!_stricmp(command, "vxCreateContext")) {
		// parse the command
		vx_uint32 vxIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: vxCreateContext() => vx[#]";
		SYNTAX_CHECK(ParseSkip(s, "()"));
		SYNTAX_CHECK(ParseSkipPattern(s, "=>"));
		SYNTAX_CHECK(ParseIndex(s, "vx", vxIndex));
		if (vxIndex >= num_openvx_context_) return Error("ERROR: OpenVX context out-of-range: expects: 0..%d", num_openvx_context_ - 1);
		if (openvx_context_[vxIndex]) return Error("ERROR: OpenVX context vx[%d] already exists", vxIndex);
		// create OpenCL context
		openvx_context_[vxIndex] = vxCreateContext();
		vx_status status = vxGetStatus((vx_reference)openvx_context_[vxIndex]);
		if (status) return Error("ERROR: vxCreateContext: failed (%d)", status);
		Message("..OpenVX context created for OpenVX vx[%d]\n", vxIndex);
	}
	else if (!_stricmp(command, "vxReleaseContext")) {
		// parse the command
		vx_uint32 vxIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: vxReleaseContext(vx[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseIndex(s, "vx", vxIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (vxIndex >= num_openvx_context_) return Error("ERROR: OpenVX context out-of-range: expects: 0..%d", num_openvx_context_ - 1);
		if (!openvx_context_[vxIndex]) return Error("ERROR: OpenVX context vx[%d] doesn't exist", vxIndex);
		// process the command
		vx_status status = vxReleaseContext(&openvx_context_[vxIndex]);
		if (status) return Error("ERROR: vxReleaseContext(vx[%d]) failed (%d)", vxIndex, status);
		Message("..OpenVX context for OpenVX vx[%d]\n", vxIndex);
	}
	else if (!_stricmp(command, "clCreateContext")) {
		// parse the command
		char platform[64], device[64];
		vx_uint32 clIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: clCreateContext(<platform#>|\"<platform-name>\",<device#>|\"<device-name>\") => cl[#]";
		SYNTAX_CHECK(ParseSkip(s, "("));
		if (*s == '"') {
			SYNTAX_CHECK(ParseString(s, platform, sizeof(platform)));
		}
		else {
			SYNTAX_CHECK(ParseWord(s, platform, sizeof(platform)));
		}
		SYNTAX_CHECK(ParseSkip(s, ","));
		if (*s == '"') {
			SYNTAX_CHECK(ParseString(s, device, sizeof(device)));
		}
		else {
			SYNTAX_CHECK(ParseWord(s, device, sizeof(device)));
		}
		SYNTAX_CHECK(ParseSkip(s, ")"));
		SYNTAX_CHECK(ParseSkipPattern(s, "=>"));
		SYNTAX_CHECK(ParseIndex(s, "cl", clIndex));
		if (clIndex >= num_opencl_context_) return Error("ERROR: OpenCL context out-of-range: expects: 0..%d", num_opencl_context_ - 1);
		if (opencl_context_[clIndex]) return Error("ERROR: OpenCL context cl[%d] already exists", clIndex);
		// get OpenCL platform ID
		cl_platform_id platform_id[16]; cl_uint num_platform_id = 0;
		cl_int err = clGetPlatformIDs(16, platform_id, &num_platform_id);
		if (err) return Error("ERROR: clGetPlatformIDs failed (%d)", err);
		if (num_platform_id < 1) return -1;
		bool found = false; cl_uint platform_index = 0; char name[256] = "invalid";
		if (platform && platform[0] >= '0' && platform[0] <= '9') {
			platform_index = (cl_uint)atoi(platform);
			if (platform_index < num_platform_id) {
				err = clGetPlatformInfo(platform_id[platform_index], CL_PLATFORM_VENDOR, sizeof(name), name, NULL);
				if (err) return Error("ERROR: clGetPlatformInfo failed (%d)", err);
				found = true;
			}
		}
		else {
			for (platform_index = 0; platform_index < num_platform_id; platform_index++) {
				err = clGetPlatformInfo(platform_id[platform_index], CL_PLATFORM_VENDOR, sizeof(name), name, NULL);
				if (err) return Error("ERROR: clGetPlatformInfo failed (%d)", err);
				if (!platform || strstr(name, platform)) {
					found = true;
					break;
				}
			}
		}
		if (!found) return Error("ERROR: specified platform doesn't exist in this system: (%s)", platform);
		// get platform name and check if DirectGMA can be is supported
		clEnqueueWaitSignalAMD_fn clEnqueueWaitSignalAMD = (clEnqueueWaitSignalAMD_fn)clGetExtensionFunctionAddressForPlatform(platform_id[platform_index], "clEnqueueWaitSignalAMD");
		clEnqueueWriteSignalAMD_fn clEnqueueWriteSignalAMD = (clEnqueueWriteSignalAMD_fn)clGetExtensionFunctionAddressForPlatform(platform_id[platform_index], "clEnqueueWriteSignalAMD");
		clEnqueueMakeBuffersResidentAMD_fn clEnqueueMakeBuffersResidentAMD = (clEnqueueMakeBuffersResidentAMD_fn)clGetExtensionFunctionAddressForPlatform(platform_id[platform_index], "clEnqueueMakeBuffersResidentAMD");
		bool direct_gma_available = false;
		if (clEnqueueWaitSignalAMD && clEnqueueWriteSignalAMD && clEnqueueMakeBuffersResidentAMD) {
			direct_gma_available = true;
		}
		Message("..OpenCL platform#%d: %s %s\n", platform_index, name, direct_gma_available ? "[DirectGMA-OK]" : "[DirectGMA-No]");
		// get OpenCL device
		cl_device_id device_id[16]; cl_uint num_device_id = 0;
		err = clGetDeviceIDs(platform_id[platform_index], CL_DEVICE_TYPE_GPU, 16, device_id, &num_device_id);
		if (err) return Error("ERROR: clGetDeviceIDs failed (%d)", err);
		if (num_device_id < 1) return Error("ERROR: clGetDeviceIDs returned ZERO device IDs");
		found = false; cl_uint device_index = 0; strcpy(name, "invalid");
		if (device && device[0] >= '0' && device[0] <= '9') {
			device_index = (cl_uint)atoi(device);
			if (device_index < num_device_id) {
				clGetDeviceInfo(device_id[device_index], CL_DEVICE_NAME, sizeof(name), name, NULL);
				if (err) return Error("ERROR: clGetDeviceInfo failed (%d)", err);
				found = true;
			}
		}
		else {
			for (device_index = 0; device_index < num_device_id; device_index++) {
				clGetDeviceInfo(device_id[device_index], CL_DEVICE_NAME, sizeof(name), name, NULL);
				if (err) return Error("ERROR: clGetDeviceInfo failed (%d)", err);
				if (!device || !strcmp(name, device)) {
					found = true;
					break;
				}
			}
		}
		if (!found) return Error("ERROR: specified device doesn't exist in this system: (%s)", device);
		// get device name and check if DirectGMA is supported
		char ext[4096] = { 0 };
		err = clGetDeviceInfo(device_id[device_index], CL_DEVICE_EXTENSIONS, sizeof(ext) - 1, ext, NULL);
		if (err) return Error("ERROR: clGetDeviceInfo failed (%d)", err);
		if (!strstr(ext, "cl_amd_bus_addressable_memory"))
			direct_gma_available = false;
		printf("..OpenCL device#%d: %s %s\n", device_index, name, direct_gma_available ? "[DirectGMA-OK]" : "[DirectGMA-No]");
		// create OpenCL context
		cl_context_properties ctx_properties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id[platform_index], 0 };
		opencl_context_[clIndex] = clCreateContext(ctx_properties, 1, &device_id[device_index], NULL, NULL, &err);
		if (!opencl_context_[clIndex]) return Error("ERROR: clCreateContext: failed (%d)", err);
		opencl_cmd_queue_[clIndex] = clCreateCommandQueueWithProperties(opencl_context_[clIndex], device_id[device_index], 0, &err);
		if (!opencl_cmd_queue_[clIndex]) return Error("ERROR: clCreateCommandQueueWithProperties: failed (%d)", err);
		Message("..OpenCL context and cmd_queue created for OpenCL cl[%d]\n", clIndex);
	}
	else if (!_stricmp(command, "clReleaseContext")) {
		// parse the command
		vx_uint32 clIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: clReleaseContext(cl[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseIndex(s, "cl", clIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (clIndex >= num_opencl_context_) return Error("ERROR: OpenCL context out-of-range: expects: 0..%d", num_opencl_context_ - 1);
		if (!opencl_context_[clIndex]) return Error("ERROR: OpenCL context cl[%d] doesn't exist", clIndex);
		// process the command
		if (opencl_cmd_queue_[clIndex]) {
			cl_int status = clReleaseCommandQueue(opencl_cmd_queue_[clIndex]);
			if (status) return Error("ERROR: clReleaseCommandQueue(cl[%d]) failed (%d)", clIndex, status);
			opencl_cmd_queue_[clIndex] = nullptr;
		}
		cl_int status = clReleaseContext(opencl_context_[clIndex]);
		if (status) return Error("ERROR: clReleaseContext(cl[%d]) failed (%d)", clIndex, status);
		opencl_context_[clIndex] = nullptr;
		Message("..OpenCL context and cmd_queue released for OpenCL cl[%d]\n", clIndex);
	}
	else if (!_stricmp(command, "clCreateBuffer")) {
		// parse the command
		vx_uint32 clIndex = 0, bufIndex = 0, bufSize = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: clCreateBuffer(cl[#],<size-in-bytes>) => buf[#]";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseIndex(s, "cl", clIndex));
		SYNTAX_CHECK(ParseSkip(s, ","));
		SYNTAX_CHECK(ParseUInt(s, bufSize));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		SYNTAX_CHECK(ParseSkipPattern(s, "=>"));
		SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
		if (clIndex >= num_opencl_context_) return Error("ERROR: OpenCL context out-of-range: expects: 0..%d", num_opencl_context_ - 1);
		if (!opencl_context_[clIndex]) return Error("ERROR: OpenCL context cl[%d] doesn't exist", clIndex);
		if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		if (opencl_buf_mem_[bufIndex]) return Error("ERROR: OpenCL buffer buf[%d] already exists", bufIndex);
		// create OpenCL buffer
		cl_int err;
		opencl_buf_mem_[bufIndex] = clCreateBuffer(opencl_context_[clIndex], CL_MEM_READ_WRITE, bufSize, NULL, &err);
		if (!opencl_buf_mem_[bufIndex]) return Error("ERROR: clCreateBuffer(...,%d,...): failed (%d)", bufSize, err);
		opencl_buf_size_[bufIndex] = bufSize;
		opencl_buf_cmdq_[bufIndex] = opencl_cmd_queue_[clIndex];
		Message("..OpenCL buffer buf[%d] created from OpenCL cl[%d]\n", bufIndex, clIndex);
	}
	else if (!_stricmp(command, "clReleaseMemObject")) {
		// parse the command
		vx_uint32 bufIndex = 0;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: clReleaseMemObject(buf[#])";
		SYNTAX_CHECK(ParseSkip(s, "("));
		SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
		SYNTAX_CHECK(ParseSkip(s, ")"));
		if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		if (!opencl_buf_mem_[bufIndex]) return Error("ERROR: OpenCL context cl[%d] doesn't exist", bufIndex);
		// process the command
		cl_int status = clReleaseMemObject(opencl_buf_mem_[bufIndex]);
		if (status) return Error("ERROR: clReleaseMemObject(buf[%d]) failed (%d)", bufIndex, status);
		opencl_buf_mem_[bufIndex] = nullptr;
		Message("..OpenCL buffer release for OpenCL buffer buf[%d]\n", bufIndex);
	}
	else if (!_stricmp(command, "load-buf") || !_stricmp(command, "load-bmp") || !_stricmp(command, "load-bmps")) {
		bool loadBMP = !_strnicmp(command, "load-bmp", 8) ? true : false;
		bool loadBMPs = !_stricmp(command, "load-bmps") ? true : false;
		// parse the command
		vx_uint32 bufIndex = 0; char fileName[256];
		const char * invalidSyntax = loadBMP ?
			"ERROR: invalid syntax: expects: load-bmp buf[#] \"fileName.bmp\" width height stride_in_bytes" :
			"ERROR: invalid syntax: expects: load-buf buf[#] \"fileName.bin\"";
		invalidSyntax = loadBMPs ?
			"ERROR: invalid syntax: expects: load-bmps buf[#] \"<fileNameFormat>.bmp\" width height num_rows num_columns stride_in_bytes" :
			invalidSyntax;
		vx_uint32 width = 0, height = 0, columns = 1, rows = 1, stride_in_bytes = 0;
		SYNTAX_CHECK(ParseSkip(s, ""));
		SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
		SYNTAX_CHECK(ParseSkip(s, ""));
		SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
		if (loadBMP) {
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, width));
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, height));
		}
		if (loadBMPs) {
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, rows));
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, columns));
			if (rows < 1 || columns < 1 || (width % columns) || (height % rows)) return Error("ERROR: invalid numbers of rows and columns specified\n");
		}
		if (loadBMP) {
			SYNTAX_CHECK(ParseSkip(s, ""));
			if (*s >= '0' && *s <= '9') {
				SYNTAX_CHECK(ParseUInt(s, stride_in_bytes));
				if (stride_in_bytes && stride_in_bytes < width * 3) return Error("ERROR: stride is smaller than expected: must be >= 3*width\n");
			}
		}
		if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		if (!opencl_buf_mem_[bufIndex]) return Error("ERROR: OpenCL buffer buf[%d] doesn't exist", bufIndex);
		if (loadBMP) {
			if (!stride_in_bytes) stride_in_bytes = width * 3;
			if ((height * stride_in_bytes) > opencl_buf_size_[bufIndex]) {
				return Error("ERROR: The specified BMP dimensions %dx%d are too large to fit in buf[%d]\n", width, height, bufIndex);
			}
		}
		// process the command
		cl_int err = clFinish(opencl_buf_cmdq_[bufIndex]);
		if (err) return Error("ERROR: clFinish failed (%d)", err);
		unsigned char * img = (unsigned char *)clEnqueueMapBuffer(opencl_buf_cmdq_[bufIndex], opencl_buf_mem_[bufIndex], CL_TRUE, CL_MAP_WRITE, 0, opencl_buf_size_[bufIndex], 0, NULL, NULL, &err);
		if (err) return Error("ERROR: clEnqueueMapBuffer failed (%d)", err);
		err = clFinish(opencl_buf_cmdq_[bufIndex]);
		if (err) return Error("ERROR: clFinish failed (%d)", err);
		if (loadBMP) {
			width /= columns;
			height /= rows;
			unsigned char * buf = nullptr;
			for (vx_uint32 row = 0, camIndex = 0; row < rows; row++) {
				for (vx_uint32 column = 0; column < columns; column++, camIndex++) {
					char bmpFileName[256]; sprintf(bmpFileName, fileName, camIndex);
					FILE * fp = fopen(bmpFileName, "rb"); if (!fp) return Error("ERROR: unable to open: %s", bmpFileName);
					unsigned short bmpHeader[54 / 2];
					fread(bmpHeader, 1, sizeof(bmpHeader), fp);
					if (width != (vx_uint32)bmpHeader[9] || height != (vx_uint32)bmpHeader[11]) {
						return Error("ERROR: The BMP should be %dx%d: got %dx%d\n", width, height, bmpHeader[9], bmpHeader[11]);
					}
					if (bmpHeader[0] != 19778 || bmpHeader[5] != 54 || ((bmpHeader[2] << 16) + bmpHeader[1] - bmpHeader[5]) != width * height * 3) {
						return Error("ERROR: The BMP format is not supported or dimensions doesn't match buffer size\n");
					}
					if(!buf) buf = new unsigned char[width * 3];
					for (vx_uint32 y = 0; y < height; y++) {
						fread(buf, 1, width * 3, fp);
						unsigned char * q = img + (row * height + height - 1 - y)*stride_in_bytes + column * width * 3, *p = buf;
						for (vx_uint32 x = 0; x < width; x++, p += 3, q += 3) {
							q[0] = p[2];
							q[1] = p[1];
							q[2] = p[0];
						}
					}
					fclose(fp);
				}
			}
			delete[] buf;
		}
		else {
			FILE * fp = fopen(fileName, "rb"); if (!fp) return Error("ERROR: unable to open: %s", fileName);
			vx_uint32 n = (vx_uint32)fread(img, 1, opencl_buf_size_[bufIndex], fp);
			if (n != opencl_buf_size_[bufIndex]) return Error("ERROR: couldn't read %d bytes from %s", n, fileName);
			fclose(fp);
		}
		err = clEnqueueUnmapMemObject(opencl_buf_cmdq_[bufIndex], opencl_buf_mem_[bufIndex], img, 0, NULL, NULL);
		if (err) return Error("ERROR: clEnqueueUnmapMemObject failed (%d)", err);
		err = clFinish(opencl_buf_cmdq_[bufIndex]);
		if (err) return Error("ERROR: clFinish failed (%d)", err);
		Message("..%s buf[%d] %s successful\n", command, bufIndex, fileName);
	}
	else if (!_stricmp(command, "save-buf") || !_stricmp(command, "save-bmp") || !_stricmp(command, "save-bmps")) {
		bool saveBMP = !_strnicmp(command, "save-bmp", 8) ? true : false;
		bool saveBMPs = !_stricmp(command, "save-bmps") ? true : false;
		// parse the command
		vx_uint32 bufIndex = 0, width = 0, height = 0, columns = 1, rows = 1, stride_in_bytes = 0;
		char fileName[256];
		const char * invalidSyntax = saveBMP ?
			"ERROR: invalid syntax: expects: save-bmp buf[#] \"fileName.bmp\" width height stride_in_bytes" :
			"ERROR: invalid syntax: expects: save-buf buf[#] \"fileName.bin\"";
		invalidSyntax = saveBMPs ?
			"ERROR: invalid syntax: expects: save-bmps buf[#] \"<fileNameFormat>.bmp\" width height num_rows num_columns stride_in_bytes" :
			invalidSyntax;
		SYNTAX_CHECK(ParseSkip(s, ""));
		SYNTAX_CHECK(ParseIndex(s, "buf", bufIndex));
		SYNTAX_CHECK(ParseSkip(s, ""));
		SYNTAX_CHECK(ParseString(s, fileName, sizeof(fileName)));
		if (saveBMP) {
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, width));
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, height));
			if (width < 1 || height < 1) return Error("ERROR: invalid width and height specified: must be > 0\n");
		}
		if (saveBMPs) {
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, rows));
			SYNTAX_CHECK(ParseSkip(s, ""));
			SYNTAX_CHECK(ParseUInt(s, columns));
			if (rows < 1 || columns < 1 || (width % columns) || (height % rows)) return Error("ERROR: invalid numbers of rows and columns specified\n");
		}
		if (saveBMP) {
			SYNTAX_CHECK(ParseSkip(s, ""));
			if (*s >= '0' && *s <= '9') {
				SYNTAX_CHECK(ParseUInt(s, stride_in_bytes));
				if (stride_in_bytes && stride_in_bytes < width * 3) return Error("ERROR: stride is smaller than expected: must be >= 3*width\n");
			}
		}
		if (bufIndex >= num_opencl_buf_) return Error("ERROR: OpenCL buffer out-of-range: expects: 0..%d", num_opencl_buf_ - 1);
		if (!opencl_buf_mem_[bufIndex]) return Error("ERROR: OpenCL context buf[%d] doesn't exist", bufIndex);
		if (saveBMP) {
			if (!stride_in_bytes) stride_in_bytes = width * 3;
			if ((height * stride_in_bytes) > opencl_buf_size_[bufIndex]) {
				return Error("ERROR: The specified BMP dimensions %dx%d are larger than buf[%d]\n", width, height, bufIndex);
			}
		}
		// process the command
		cl_int err = clFinish(opencl_buf_cmdq_[bufIndex]);
		if (err) return Error("ERROR: clFinish failed (%d)", err);
		unsigned char * img = (unsigned char *)clEnqueueMapBuffer(opencl_buf_cmdq_[bufIndex], opencl_buf_mem_[bufIndex], CL_TRUE, CL_MAP_READ, 0, opencl_buf_size_[bufIndex], 0, NULL, NULL, &err);
		if (err) return Error("ERROR: clEnqueueMapBuffer failed (%d)", err);
		err = clFinish(opencl_buf_cmdq_[bufIndex]);
		if (err) return Error("ERROR: clFinish failed (%d)", err);
		if (saveBMP) {
			width /= columns;
			height /= rows;
			unsigned char * buf = new unsigned char[width * 3];
			for (vx_uint32 row = 0, camIndex = 0; row < rows; row++) {
				for (vx_uint32 column = 0; column < columns; column++, camIndex++) {
					char bmpFileName[256]; sprintf(bmpFileName, fileName, camIndex);
					FILE * fp = fopen(bmpFileName, "wb"); if (!fp) return Error("ERROR: unable to create: %s", bmpFileName);
					vx_uint32 size = 3 * width * height;
					short bmpHeader[54 / 2] = {
						19778, (short)((size + 54) & 0xffff), (short)((size + 54) >> 16), 0, 0, 54, 0, 40, 0,
						(short)width, 0, (short)height, 0, 1, 24, 0, 0,
						(short)(size & 0xffff), (short)(size >> 16), 0, 0, 0, 0, 0, 0, 0, 0
					};
					fwrite(bmpHeader, 1, sizeof(bmpHeader), fp);
					for (vx_uint32 y = 0; y < height; y++) {
						unsigned char * p = img + (height * row + height - 1 - y)*stride_in_bytes + column * width * 3, *q = buf;
						for (vx_uint32 x = 0; x < width; x++, p += 3, q += 3) {
							q[0] = p[2];
							q[1] = p[1];
							q[2] = p[0];
						}
						fwrite(buf, 1, width * 3, fp);
					}
					fclose(fp);
					Message("..%s buf[%d] created %s\n", command, bufIndex, bmpFileName);
				}
			}
			delete[] buf;
		}
		else {
			FILE * fp = fopen(fileName, "wb"); if (!fp) return Error("ERROR: unable to create: %s", fileName);
			fwrite(img, 1, opencl_buf_size_[bufIndex], fp);
			fclose(fp);
			Message("..%s buf[%d] %s successful\n", command, bufIndex, fileName);
		}
		err = clEnqueueUnmapMemObject(opencl_buf_cmdq_[bufIndex], opencl_buf_mem_[bufIndex], img, 0, NULL, NULL);
		if (err) return Error("ERROR: clEnqueueUnmapMemObject failed (%d)", err);
		err = clFinish(opencl_buf_cmdq_[bufIndex]);
		if (err) return Error("ERROR: clFinish failed (%d)", err);
	}
	else if (!_stricmp(command, "process")) {
		// parse the command
		vx_uint32 contextIndex = 0, numFrames = 1;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: process ls[#] <num-frames>|live";
		SYNTAX_CHECK(ParseSkip(s, ""));
		SYNTAX_CHECK(ParseContextWithErrorCheck(s, contextIndex, invalidSyntax));
		SYNTAX_CHECK(ParseSkip(s, ""));
		if (s[0] >= '1' && s[0] <= '9') {
			SYNTAX_CHECK(ParseUInt(s, numFrames));
		}
		else {
			SYNTAX_CHECK(ParseSkipPattern(s, "live"));
			numFrames = 0;
		}
		// process the command
		vx_status status = VX_SUCCESS;
		vx_uint32 count = 0;
		for (vx_uint32 i = 0; numFrames == 0 || i < numFrames; i++, count++) {
			vx_status status = lsScheduleFrame(context_[contextIndex]);
			if (status == VX_ERROR_GRAPH_ABANDONED) break;
			if (status) return Error("ERROR: lsScheduleFrame(ls[%d]) failed (%d) @iter:%d", contextIndex, status, i);
			status = lsWaitForCompletion(context_[contextIndex]);
			if (status == VX_ERROR_GRAPH_ABANDONED) break;
			if (status) return Error("ERROR: lsWaitForCompletion(ls[%d]) failed (%d) @iter:%d", contextIndex, status, i);
		}
		if (status) Message("..process: execution abandoned after %d frames from ls[%d]\n", count, contextIndex);
		else        Message("..process: executed for %d frames from ls[%d]\n", count, contextIndex);
	}
	else if (!_stricmp(command, "process-all")) {
		// parse the command
		vx_uint32 numFrames = 1;
		const char * invalidSyntax = "ERROR: invalid syntax: expects: process-all <num-frames>|live";
		SYNTAX_CHECK(ParseSkip(s, ""));
		if (s[0] >= '1' && s[0] <= '9') {
			SYNTAX_CHECK(ParseUInt(s, numFrames));
		}
		else {
			SYNTAX_CHECK(ParseSkipPattern(s, "live"));
			numFrames = 0;
		}
		// process the command
		vx_status status = VX_SUCCESS;
		vx_uint32 count = 0;
		for (vx_uint32 i = 0; numFrames == 0 || i < numFrames; i++, count++) {
			for (vx_uint32 j = 0; j < num_context_; j++) {
				if (context_[j]) {
					status = lsScheduleFrame(context_[j]);
					if (status == VX_ERROR_GRAPH_ABANDONED) break;
					if (status) return Error("ERROR: lsScheduleFrame(ls[%d]) failed (%d) @iter:%d", j, status, i);
				}
			}
			if (status) break;
			for (vx_uint32 j = 0; j < num_context_; j++) {
				if (context_[j]) {
					status = lsWaitForCompletion(context_[j]);
					if (status == VX_ERROR_GRAPH_ABANDONED) break;
					if (status) return Error("ERROR: lsWaitForCompletion(ls[%d]) failed (%d) @iter:%d", j, status, i);
				}
			}
			if (status) break;
		}
		if (status) Message("..process-all: execution abandoned after %d frames\n", count);
		else        Message("..process-all: executed for %d frames\n", count);
	}
	else if (!_stricmp(command, "help")) {
		Message("..help: context\n");
		Message("    lsCreateContext() => ls[#]\n");
		Message("    lsReleaseContext(ls[#])\n");
		Message("..help: rig and image configuration\n");
		Message("    lsSetOutputConfig(ls[#],format,width,height)\n");
		Message("    lsSetCameraConfig(ls[#],num_rows,num_cols,format,width,height)\n");
		Message("    lsSetCameraParams(ls[#],index,{{yaw,pitch,roll,tx,ty,tz},{lens,haw,hfov,k1,k2,k3,du0,dv0,r_crop}})\n");
		Message("    lsSetOverlayConfig(ls[#],num_rows,num_cols,format,width,height)\n");
		Message("    lsSetOverlayParams(ls[#],index,{{yaw,pitch,roll,tx,ty,tz},{lens,haw,hfov,k1,k2,k3,du0,dv0,r_crop}})\n");
		Message("    lsSetRigParams(ls[#],{yaw,pitch,roll,d})\n");
		Message("    lsGetOutputConfig(ls[#])\n");
		Message("    lsGetCameraConfig(ls[#])\n");
		Message("    lsGetCameraParams(ls[#],index)\n");
		Message("    lsGetOverlayConfig(ls[#])\n");
		Message("    lsGetOverlayParams(ls[#],index)\n");
		Message("    lsGetRigParams(ls[#])\n");
		Message("..help: import/export configuration\n");
		Message("    lsExportConfiguration(ls[#],\"<exportType>\",\"<fileName>\")\n");
		Message("    lsImportConfiguration(ls[#],\"<importType>\",\"<fileName>\")\n");
		Message("..help: LoomIO configuration\n");
		Message("    lsSetCameraModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")\n");
		Message("    lsSetOutputModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")\n");
		Message("    lsSetOverlayModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")\n");
		Message("    lsSetViewingModule(ls[#],\"module\",\"kernelName\",\"kernelArguments\")\n");
		Message("    lsGetCameraModule(ls[#])\n");
		Message("    lsGetOutputModule(ls[#])\n");
		Message("    lsGetOverlayModule(ls[#])\n");
		Message("    lsGetViewingModule(ls[#])\n");
		Message("..help: initialize and schedule\n");
		Message("    lsInitialize(ls[#])\n");
		Message("    lsScheduleFrame(ls[#])\n");
		Message("    lsWaitForCompletion(ls[#])\n");
		Message("    process ls[#] <num-frames>|live\n");
		Message("    process-all <num-frames>|live\n");
		Message("..help: image I/O configuration (not supported with LoomIO)\n");
		Message("    lsSetCameraBufferStride(ls[#],stride_in_bytes)\n");
		Message("    lsSetOutputBufferStride(ls[#],stride_in_bytes)\n");
		Message("    lsSetOverlayBufferStride(ls[#],stride_in_bytes)\n");
		Message("    lsSetCameraBuffer(ls[#],buf[#]|NULL)\n");
		Message("    lsSetOutputBuffer(ls[#],buf[#]|NULL)\n");
		Message("    lsSetOverlayBuffer(ls[#],buf[#]|NULL)\n");
		Message("    lsGetCameraBufferStride(ls[#])\n");
		Message("    lsGetOutputBufferStride(ls[#])\n");
		Message("    lsGetOverlayBufferStride(ls[#])\n");
		Message("..help: OpenCL buffers\n");
		Message("    clCreateBuffer(cl[#],<size-in-bytes>) => buf[#]\n");
		Message("    clReleaseMemObject(buf[#])\n");
		Message("    load-buf buf[#] \"fileName.bin\"\n");
		Message("    save-buf buf[#] \"fileName.bin\"\n");
		Message("    load-bmp buf[#] \"fileName.bmp\" width height stride_in_bytes\n");
		Message("    save-bmp buf[#] \"fileName.bmp\" width height stride_in_bytes\n");
		Message("    load-bmps buf[#] \"<fileNameFormat>.bmp\" width height num_rows num_columns stride_in_bytes\n");
		Message("    save-bmps buf[#] \"<fileNameFormat>.bmp\" width height num_rows num_columns stride_in_bytes\n");
		Message("..help: OpenCL context (advanced)\n");
		Message("    lsGetOpenCLContext(ls[#],cl[#])\n");
		Message("    clCreateContext(<platform#>|\"<platform-name>\",<device#>|\"<device-name>\") => cl[#]\n");
		Message("    clReleaseContext(cl[#])\n");
		Message("    lsSetOpenCLContext(ls[#],cl[#])\n");
		Message("..help: OpenVX context (advanced)\n");
		Message("    lsGetOpenVXContext(ls[#],vx[#])\n");
		Message("    vxCreateContext() => vx[#]\n");
		Message("    vxReleaseContext(vx[#])\n");
		Message("    lsSetOpenVXContext(ls[#],vx[#])\n");
		Message("..help: attributes (advanced)\n");
		Message("    lsGlobalSetAttributes(offset,count,\"attr.txt\")\n");
		Message("    lsGlobalGetAttributes(offset,count,\"attr.txt\")\n");
		Message("    lsGlobalSetAttributes(offset,count,{value(s)})\n");
		Message("    lsGlobalGetAttributes(offset,count)\n");
		Message("    lsSetAttributes(ls[#],offset,count,\"attr.txt\")\n");
		Message("    lsGetAttributes(ls[#],offset,count,\"attr.txt\")\n");
		Message("    lsSetAttributes(ls[#],offset,count,{value(s)})\n");
		Message("    lsGetAttributes(ls[#],offset,count)\n");
	}
	else return Error("ERROR: invalid command: '%s'", command);
	return 0;
#undef SYNTAX_CHECK
#undef SYNTAX_CHECK_WITH_MESSAGE
}

CCommandLineParser::CCommandLineParser()
{
	verbose_ = false;
	lineNum_ = 0;
	fileName_ = nullptr;
}

void CCommandLineParser::SetVerbose(bool verbose)
{
	verbose_ = verbose;
}

bool CCommandLineParser::Verbose()
{
	return verbose_;
}

void CCommandLineParser::Message(const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	fflush(stdout);
}

int CCommandLineParser::Error(const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf(" @%s#%d\n", fileName_ ? fileName_ : "console", lineNum_);
	fflush(stdout);
	return -1;
}

void CCommandLineParser::Terminate(int code, const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	exit(code);
}

bool CCommandLineParser::GetCommand(FILE * fp)
{
	cmd_ = "";
	int c = EOF;
	enum { BEGIN, MIDDLE, SPACE, ESCAPE, COMMENT } state = BEGIN;
	if (fp == stdin) Message("> ");
	while ((c = getc(fp)) != EOF) {
		// increment lineNum
		if (c == '\n') lineNum_++;
		// process begin, space, escape, and comments
		if (state == BEGIN) {
			if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
				// skip white space
				continue;
			}
			state = MIDDLE;
		}
		else if (state == ESCAPE) {
			if (c == '\r') {
				// skip CR and anticipate LF
				state = ESCAPE;
				continue;
			}
			else if (c == '\n') {
				// skip LF
				state = (cmd_.length() > 0) ? MIDDLE : BEGIN;
				continue;
			}
			state = MIDDLE;
		}
		else if (state == COMMENT) {
			if (c == '\n')
				state = (cmd_.length() > 0) ? MIDDLE : BEGIN;
			continue;
		}
		else if (state == SPACE) {
			if (c == ' ' || c == '\t' || c == '\r')
				continue;
			else if (c == '\\') {
				state = ESCAPE;
				continue;
			}
			else if (c == '#') {
				state = COMMENT;
				continue;
			}
			else if (c == '\n') {
				break;
			}
			state = MIDDLE;
			cmd_ += " ";
		}
		// detect space, escape, comments, and end-of-line
		if (c == '\\') {
			state = ESCAPE;
			continue;
		}
		else if (c == '#') {
			state = COMMENT;
			continue;
		}
		else if (c == ' ' || c == '\t' || c == '\r') {
			state = SPACE;
			continue;
		}
		else if (c == '\n') {
			break;
		}
		// add character to cmd
		cmd_ += c;
	}
	return cmd_.length() > 0 ? true : false;
}

int CCommandLineParser::Run(const char * fileName)
{
	int status = 0;

	// open the input script file (or use stdin)
	FILE * fp = stdin;
	if (fileName) fp = fopen(fileName, "r");
	if (!fp) Terminate(1, "ERROR: unable to open: %s\n", fileName);
	Message("... processing commands from %s\n", fileName ? fileName : "console");

	// process one command at a time
	lineNum_ = 0;
	fileName_ = fileName;
	while (GetCommand(fp)) {
		const char * cmd = cmd_.c_str();
		// verbose
		if (verbose_) Message("> %s\n", cmd);
		// process built-in commands
		if (!_stricmp(cmd, "verbose on")) {
			Message("... verbose ON\n");
			SetVerbose(true);
		}
		else if (!_stricmp(cmd, "verbose off")) {
			Message("... verbose OFF\n");
			SetVerbose(false);
		}
		else if (!_stricmp(cmd, "quit")) {
			Message("... quit from %s\n", fileName ? fileName : "console");
			exit(0);
		}
		else if (!_stricmp(cmd, "exit")) {
			break;
		}
		else if (!_strnicmp(cmd, "run", 3)) {
			// continue running script and make sure to save and restore fileName_ and lineNum_ for Error()
			int lineNum = lineNum_;
			char scriptFileName[256] = { 0 };
			if (cmd[3] == ' ') strncpy(scriptFileName, &cmd[4], sizeof(scriptFileName) - 1);
			status = Run(scriptFileName[0] ? scriptFileName : nullptr);
			fileName_ = fileName;
			lineNum_ = lineNum;
			// check for error
			if (status < 0)
				break;
		}
		else if (!_stricmp(cmd, "help")) {
			Message("..help: command-line usage\n");
			Message("    %% %s [script%s]\n", PROGRAM_NAME, SCRIPT_EXTENSION);
			Message("..help: basic commands\n");
			Message("    help\n");
			Message("    verbose ON\n");
			Message("    verbose OFF\n");
			Message("    run script%s\n", SCRIPT_EXTENSION);
			Message("    exit\n");
			Message("    quit\n");
			OnCommand();
			Message("..help: end\n");
		}
		else {
			// invoke command processor
			status = OnCommand();
			if (status < 0)
				break;
		}
	}

	// close input script file
	if (fp != stdin) fclose(fp);
	Message("... exit from %s\n", fileName ? fileName : "console");
	return status;
}

int CCommandLineParser::OnCommand()
{
	return 0;
}

int main(int argc, char *argv[])
{
	printf("%s %s [loomsl %s]\n", PROGRAM_NAME, VERSION, lsGetVersion());
	CLoomShellParser parser;
	return parser.Run(argv[1]);
}
