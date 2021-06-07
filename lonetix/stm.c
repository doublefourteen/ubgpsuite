// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file stm.c
 *
 * Common `StmOps`implementation over `Fildes`.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "stm.h"

#include "sys/fs.h"

#define STM_TOFILDES(fd) ((Fildes) ((Sintptr) (fd)))

static Sint64 Stm_Fread(void *streamp, void *buf, size_t nbytes)
{
	return Sys_Fread(STM_TOFILDES(streamp), buf, nbytes);
}

static Sint64 Stm_Fwrite(void *streamp, const void *buf, size_t nbytes)
{
	return Sys_Fwrite(STM_TOFILDES(streamp), buf, nbytes);
}

static Sint64 Stm_Fseek(void *streamp, Sint64 off, SeekMode whence)
{
	return Sys_Fseek(STM_TOFILDES(streamp), off, whence);
}

static Sint64 Stm_Ftell(void *streamp)
{
	return Sys_Ftell(STM_TOFILDES(streamp));
}

static Judgement Stm_Fsync(void *streamp)
{
	return Sys_Fsync(STM_TOFILDES(streamp), /*fullSync=*/TRUE);
}

static void Stm_Fclose(void *streamp)
{
	Sys_Fclose(STM_TOFILDES(streamp));
}

static const StmOps fildes_ops = {
	Stm_Fread,
	Stm_Fwrite,
	Stm_Fseek,
	Stm_Ftell,
	Stm_Fsync,
	Stm_Fclose
};

static const StmOps nc_fildes_ops = {
	Stm_Fread,
	Stm_Fwrite,
	Stm_Fseek,
	Stm_Ftell,
	Stm_Fsync,
	NULL
};

const StmOps *const Stm_FildesOps   = &fildes_ops;
const StmOps *const Stm_NcFildesOps = &nc_fildes_ops;
