/* devHpe1368a.c */
/* base/src/dev $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            09-02-92
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  08-02-92        mrk     Original version
 * .02	06-03-93	joh	fixed read_mbbi routine calls the 
 *				"at5vxi_bi_driver" rather than the 
 *				"hpe1368a_bi_driver" 
 * .03 09-01-93		joh	expect EPICS status from driver
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<link.h>
#include	<module_types.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<mbboRecord.h>

#include	<drvHpe1368a.h>


static long init_bi();
static long init_bo();
static long init_mbbi();
static long init_mbbo();
static long bi_ioinfo();
static long mbbi_ioinfo();
static long read_bi();
static long write_bo();
static long read_mbbi();
static long write_mbbo();


typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	} BINARYDSET;


BINARYDSET devBiHpe1368a=  {6,NULL,NULL,init_bi,  bi_ioinfo,  read_bi};
BINARYDSET devBoHpe1368a=  {6,NULL,NULL,init_bo,       NULL,  write_bo};
BINARYDSET devMbbiHpe1368a={6,NULL,NULL,init_mbbi,mbbi_ioinfo,read_mbbi};
BINARYDSET devMbboHpe1368a={6,NULL,NULL,init_mbbo,       NULL,write_mbbo};

static long init_bi( struct biRecord	*pbi)
{
    struct vmeio *pvmeio;


    /* bi.inp must be an VME_IO */
    switch (pbi->inp.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	pbi->mask=1;
	pbi->mask <<= pvmeio->signal;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiHpe1368a (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long bi_ioinfo(
    int               cmd,
    struct biRecord     *pbi,
    IOSCANPVT		*ppvt)
{
    return hpe1368a_getioscanpvt(pbi->inp.value.vmeio.card,ppvt);
}

static long read_bi(struct biRecord	*pbi)
{
	struct vmeio 	*pvmeio;
	long		status;
	unsigned	value;

	
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	status = hpe1368a_bi_driver(pvmeio->card,pbi->mask,&value);
	if(status==0) {
		pbi->rval = value;
	} else {
                recGblSetSevr(pbi,READ_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_bo(struct boRecord	*pbo)
{
    unsigned int value;
    long	status=0;
    struct vmeio *pvmeio;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbo->out.value);
	pbo->mask = 1;
	pbo->mask <<= pvmeio->signal;
        status = hpe1368a_bi_driver(pvmeio->card,pbo->mask,&value);
        if(status == 0){
		pbo->rbv = pbo->rval = value;
	}
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pbo,
	    "devBoHpe1368a (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_bo(struct boRecord	*pbo)
{
	struct vmeio *pvmeio;
	long		status;

	
	pvmeio = (struct vmeio *)&(pbo->out.value);
	status = hpe1368a_bo_driver(pvmeio->card,pbo->rval,pbo->mask);
	if(status!=0) {
                recGblSetSevr(pbo,WRITE_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_mbbi(struct mbbiRecord	*pmbbi)
{

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :
	pmbbi->shft = pmbbi->inp.value.vmeio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiHpe1368a (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long mbbi_ioinfo(
    int               cmd,
    struct mbbiRecord     *pmbbi,
    IOSCANPVT		*ppvt)
{
    return hpe1368a_getioscanpvt(pmbbi->inp.value.vmeio.card,ppvt);
}

static long read_mbbi(struct mbbiRecord	*pmbbi)
{
	struct vmeio	*pvmeio;
	long		status;
	unsigned 	value;

	
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	status = hpe1368a_bi_driver(pvmeio->card,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
	} else {
                recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_mbbo(struct mbboRecord	*pmbbo)
{
    unsigned 		value;
    struct vmeio 	*pvmeio;
    long		status = 0;

    /* mbbo.out must be an VME_IO */
    switch (pmbbo->out.type) {
    case (VME_IO) :
	pvmeio = &(pmbbo->out.value.vmeio);
	pmbbo->shft = pvmeio->signal;
	pmbbo->mask <<= pmbbo->shft;
	status = hpe1368a_bi_driver(pvmeio->card,pmbbo->mask,&value);
	if(status==0){
		pmbbo->rbv = pmbbo->rval = value;
	}
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboHpe1368a (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbbo(struct mbboRecord	*pmbbo)
{
	struct vmeio 	*pvmeio;
	long		status;
	unsigned 	value;

	
	pvmeio = &(pmbbo->out.value.vmeio);
	status = hpe1368a_bo_driver(pvmeio->card,pmbbo->rval,pmbbo->mask);
	if(status==0) {
		status = hpe1368a_bi_driver(pvmeio->card,pmbbo->mask,&value);
		if(status==0) pmbbo->rbv = value;
                else recGblSetSevr(pmbbo,READ_ALARM,INVALID_ALARM);
	} else {
                recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM);
	}
	return(status);
}
