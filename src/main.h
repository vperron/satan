/**
 * =====================================================================================
 *
 *   @file main.h
 *   @author Victor Perron (), victor.perron@locarise.com
 *   
 *        Version:  1.0
 *        Created:  09/20/2012 08:26:35 PM
 *        Company:  Locarise
 *
 *   @section DESCRIPTION
 *
 *       General headers
 *       
 *   @section LICENSE
 *
 *       
 *
 * =====================================================================================
 */

#include <stdio.h>

#ifndef _MAIN_H_
#define _MAIN_H_ 

#define errorLog(fmt, ...) \
	do { \
		fprintf(stderr,"%s [%s:%d] " fmt "\n", __func__,__FILE__,__LINE__, ##__VA_ARGS__); \
		fflush(stderr); \
	} while(0)

#ifdef DEBUG
#define debugLog(fmt, ...) \
	do { \
		printf("%s [%s:%d] " fmt "\n", __func__,__FILE__,__LINE__, ##__VA_ARGS__); \
		fflush(stdout); \
	} while(0)

#else
#define debugLog(fmt, ...) 
#endif

#define STATUS_OK 0
#define STATUS_ERROR -1

#define MAX_STRING_LEN 128

/*
 * GLOBAL variables declaration
 */

#endif
