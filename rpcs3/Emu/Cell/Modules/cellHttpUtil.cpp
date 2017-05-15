#include "stdafx.h"
#include "Emu/Cell/PPUModule.h"

#include "cellHttpUtil.h"

logs::channel cellHttpUtil("cellHttpUtil");

s32 cellHttpUtilParseUri(vm::ptr<CellHttpUri> uri, vm::cptr<char> str, vm::ptr<void> pool, u32 size, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilParseUri(uri=*0x%x, str=%s, pool=*0x%x, size=%d, required=*0x%x)", uri, str, pool, size, required);
	return CELL_OK;
}

s32 cellHttpUtilParseUriPath(vm::ptr<CellHttpUriPath> path, vm::cptr<char> str, vm::ptr<void> pool, u32 size, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilParseUriPath(path=*0x%x, str=%s, pool=*0x%x, size=%d, required=*0x%x)", path, str, pool, size, required);
	return CELL_OK;
}

s32 cellHttpUtilParseProxy(vm::ptr<CellHttpUri> uri, vm::cptr<char> str, vm::ptr<void> pool, u32 size, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilParseProxy(uri=*0x%x, str=%s, pool=*0x%x, size=%d, required=*0x%x)", uri, str, pool, size, required);
	return CELL_OK;
}

s32 cellHttpUtilParseStatusLine(vm::ptr<CellHttpStatusLine> resp, vm::cptr<char> str, u32 len, vm::ptr<void> pool, u32 size, vm::ptr<u32> required, vm::ptr<u32> parsedLength)
{
	cellHttpUtil.todo("cellHttpUtilParseStatusLine(resp=*0x%x, str=%s, len=%d, pool=*0x%x, size=%d, required=*0x%x, parsedLength=*0x%x)", resp, str, len, pool, size, required, parsedLength);
	return CELL_OK;
}

s32 cellHttpUtilParseHeader(vm::ptr<CellHttpHeader> header, vm::cptr<char> str, u32 len, vm::ptr<void> pool, u32 size, vm::ptr<u32> required, vm::ptr<u32> parsedLength)
{
	cellHttpUtil.todo("cellHttpUtilParseHeader(header=*0x%x, str=%s, len=%d, pool=*0x%x, size=%d, required=*0x%x, parsedLength=*0x%x)", header, str, len, pool, size, required, parsedLength);
	return CELL_OK;
}

s32 cellHttpUtilBuildRequestLine(vm::cptr<CellHttpRequestLine> req, vm::ptr<char> buf, u32 len, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilBuildRequestLine(req=*0x%x, buf=*0x%x, len=%d, required=*0x%x)", req, buf, len, required);

	if (!req->method || !req->path || !req->protocol) {
		return CELL_HTTP_UTIL_ERROR_INVALID_REQUEST;
	}

	// TODO

    const std::string& result = fmt::format("%s %s %s/%d.%d\r\n", req->method, req->path, req->protocol, req->majorVersion, req->minorVersion);
    std::memcpy(buf.get_ptr(), result.c_str(), result.size() + 1);

	return CELL_OK;
}

s32 cellHttpUtilBuildHeader(vm::cptr<CellHttpHeader> header, vm::ptr<char> buf, u32 len, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilBuildHeader(header=*0x%x, buf=*0x%x, len=%d, required=*0x%x)", header, buf, len, required);

	if (!header->name || !header->value) {
		return CELL_HTTP_UTIL_ERROR_INVALID_HEADER;
	}

	// TODO

    const std::string& result = fmt::format("%s: %s\r\n", header->name, header->value);
    std::memcpy(buf.get_ptr(), result.c_str(), result.size() + 1);

	return CELL_OK;
}

s32 cellHttpUtilBuildUri(vm::cptr<CellHttpUri> uri, vm::ptr<char> buf, u32 len, vm::ptr<u32> required, s32 flags)
{
	cellHttpUtil.todo("cellHttpUtilBuildUri(uri=*0x%x, buf=*0x%x, len=%d, required=*0x%x, flags=%d)", uri, buf, len, required, flags);

	// TODO

    const std::string& result = fmt::format("%s://%s:%s@%s:%d/%s", uri->scheme, uri->username, uri->password, uri->hostname, uri->port, uri->path);
    std::memcpy(buf.get_ptr(), result.c_str(), result.size() + 1);

	return CELL_OK;
}

s32 cellHttpUtilCopyUri(vm::ptr<CellHttpUri> dest, vm::cptr<CellHttpUri> src, vm::ptr<void> pool, u32 poolSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilCopyUri(dest=*0x%x, src=*0x%x, pool=*0x%x, poolSize=%d, required=*0x%x)", dest, src, pool, poolSize, required);
	return CELL_OK;
}

s32 cellHttpUtilMergeUriPath(vm::ptr<CellHttpUri> uri, vm::cptr<CellHttpUri> src, vm::cptr<char> path, vm::ptr<void> pool, u32 poolSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilMergeUriPath(uri=*0x%x, src=*0x%x, path=%s, pool=*0x%x, poolSize=%d, required=*0x%x)", uri, src, path, pool, poolSize, required);
	return CELL_OK;
}

s32 cellHttpUtilSweepPath(vm::ptr<char> dst, vm::cptr<char> src, u32 srcSize)
{
	cellHttpUtil.todo("cellHttpUtilSweepPath(dst=*0x%x, src=%s, srcSize=%d)", dst, src, srcSize);
	return CELL_OK;
}

s32 cellHttpUtilCopyStatusLine(vm::ptr<CellHttpStatusLine> dest, vm::cptr<CellHttpStatusLine> src, vm::ptr<void> pool, u32 poolSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilCopyStatusLine(dest=*0x%x, src=*0x%x, pool=*0x%x, poolSize=%d, required=*0x%x)", dest, src, pool, poolSize, required);
	return CELL_OK;
}

s32 cellHttpUtilCopyHeader(vm::ptr<CellHttpHeader> dest, vm::cptr<CellHttpHeader> src, vm::ptr<void> pool, u32 poolSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilCopyHeader(dest=*0x%x, src=*0x%x, pool=*0x%x, poolSize=%d, required=*0x%x)", dest, src, pool, poolSize, required);
	return CELL_OK;
}

s32 cellHttpUtilAppendHeaderValue(vm::ptr<CellHttpHeader> dest, vm::cptr<CellHttpHeader> src, vm::cptr<char> value, vm::ptr<void> pool, u32 poolSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilAppendHeaderValue(dest=*0x%x, src=*0x%x, value=%s, pool=*0x%x, poolSize=%d, required=*0x%x)", dest, src, value, pool, poolSize, required);
	return CELL_OK;
}

s32 cellHttpUtilEscapeUri(vm::ptr<char> out, u32 outSize, vm::cptr<u8> in, u32 inSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilEscapeUri(out=*0x%x, outSize=%d, in=*0x%x, inSize=%d, required=*0x%x)", out, outSize, in, inSize, required);
	return CELL_OK;
}

s32 cellHttpUtilUnescapeUri(vm::ptr<u8> out, u32 size, vm::cptr<char> in, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilUnescapeUri(out=*0x%x, size=%d, in=*0x%x, required=*0x%x)", out, size, in, required);
	return CELL_OK;
}

s32 cellHttpUtilFormUrlEncode(vm::ptr<char> out, u32 outSize, vm::cptr<u8> in, u32 inSize, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilFormUrlEncode(out=*0x%x, outSize=%d, in=*0x%x, inSize=%d, required=*0x%x)", out, outSize, in, inSize, required);
	return CELL_OK;
}

s32 cellHttpUtilFormUrlDecode(vm::ptr<u8> out, u32 size, vm::cptr<char> in, vm::ptr<u32> required)
{
	cellHttpUtil.todo("cellHttpUtilFormUrlDecode(out=*0x%x, size=%d, in=%s, required=*0x%x)", out, size, in, required);
	return CELL_OK;
}

s32 cellHttpUtilBase64Encoder(vm::ptr<char> out, vm::cptr<void> input, u32 len)
{
	cellHttpUtil.todo("cellHttpUtilBase64Encoder(out=*0x%x, input=*0x%x, len=%d)", out, input, len);
	return CELL_OK;
}

s32 cellHttpUtilBase64Decoder(vm::ptr<char> output, vm::cptr<void> in, u32 len)
{
	cellHttpUtil.todo("cellHttpUtilBase64Decoder(output=*0x%x, in=*0x%x, len=%d)", output, in, len);
	return CELL_OK;
}

DECLARE(ppu_module_manager::cellHttpUtil)("cellHttpUtil", []()
{
	REG_FUNC(cellHttpUtil, cellHttpUtilParseUri);
	REG_FUNC(cellHttpUtil, cellHttpUtilParseUriPath);
	REG_FUNC(cellHttpUtil, cellHttpUtilParseProxy);
	REG_FUNC(cellHttpUtil, cellHttpUtilParseStatusLine);
	REG_FUNC(cellHttpUtil, cellHttpUtilParseHeader);

	REG_FUNC(cellHttpUtil, cellHttpUtilBuildRequestLine);
	REG_FUNC(cellHttpUtil, cellHttpUtilBuildHeader);
	REG_FUNC(cellHttpUtil, cellHttpUtilBuildUri);

	REG_FUNC(cellHttpUtil, cellHttpUtilCopyUri);
	REG_FUNC(cellHttpUtil, cellHttpUtilMergeUriPath);
	REG_FUNC(cellHttpUtil, cellHttpUtilSweepPath);
	REG_FUNC(cellHttpUtil, cellHttpUtilCopyStatusLine);
	REG_FUNC(cellHttpUtil, cellHttpUtilCopyHeader);
	REG_FUNC(cellHttpUtil, cellHttpUtilAppendHeaderValue);

	REG_FUNC(cellHttpUtil, cellHttpUtilEscapeUri);
	REG_FUNC(cellHttpUtil, cellHttpUtilUnescapeUri);
	REG_FUNC(cellHttpUtil, cellHttpUtilFormUrlEncode);
	REG_FUNC(cellHttpUtil, cellHttpUtilFormUrlDecode);
	REG_FUNC(cellHttpUtil, cellHttpUtilBase64Encoder);
	REG_FUNC(cellHttpUtil, cellHttpUtilBase64Decoder);
});
