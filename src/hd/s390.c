#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "s390.h"

#if defined(__s390__) || defined(__s390x__)

#include <libsysfs.h>
#include <dlist.h>

#define BUSNAME "ccw"

void hd_scan_s390(hd_data_t *hd_data)
{
  hd_t* hd;
  hd_res_t* res;
  struct sysfs_bus *bus, *bus_dup;
  struct sysfs_device *curdev = NULL;
  struct sysfs_device *curdev_dup = NULL;
  struct dlist *attributes = NULL;
  struct sysfs_attribute *curattr = NULL;

  unsigned int devtype=0,devmod=0,cutype=0,cumod=0;

  if (!hd_probe_feature(hd_data, pr_s390)) return;

  hd_data->module=mod_s390;

  remove_hd_entries(hd_data);

  //sl0=sl=read_file(PROCSUBCHANNELS, 2, 0);
  bus=sysfs_open_bus(BUSNAME);
  bus_dup=sysfs_open_bus(BUSNAME);

  if (!bus || !bus_dup)
  {
    ADD2LOG("unable to open" BUSNAME "bus");
    return;
  }


  dlist_for_each_data(bus->devices, curdev, struct sysfs_device)
  {

    res=new_mem(sizeof *res);

    attributes = sysfs_get_device_attributes(curdev);
    dlist_for_each_data(attributes,curattr, struct sysfs_attribute)
    {
      if (strcmp("online",curattr->name)==0)
	res->io.enabled=atoi(curattr->value);
      else if (strcmp("cutype",curattr->name)==0)
      {
	cutype=strtol(curattr->value,NULL,16);
	cumod=strtol(index(curattr->value,'/')+1,NULL,16);
      } else if (strcmp("devtype",curattr->name)==0)
      {
	devtype=strtol(curattr->value,NULL,16);
	devmod=strtol(index(curattr->value,'/')+1,NULL,16);
      }
    }

    res->io.type=res_io;
    res->io.access=acc_rw;  /* fix-up RO/WO devices in IDs file */
    res->io.base=strtol(rindex(curdev->bus_id,'.')+1,NULL,16);

    /* Skip additional channels for multi-channel devices */
    /* This assumes that there are no gaps between the channels of a single device. */
    if(cutype == 0x1731 || cutype == 0x3088)
    {
      int skip=0;
      dlist_for_each_data(bus_dup->devices,curdev_dup, struct sysfs_device)
      {
	int tmp=strtol(rindex(curdev_dup->bus_id,'.')+1,NULL,16);
	fprintf(stderr,"tmp is %d\n",tmp);
	if(tmp == res->io.base - 1)
	  skip=1;
	if(cutype == 0x1731 && tmp == res->io.base - 2)
	  skip=1;
      }
      if(skip) continue;
    }

    res->io.range=1;
    switch (cutype)
    {
      /* three channels */
      case 0x1731:    /* QDIO (QETH, HSI, zFCP) */
	res->io.range++;
      /* two channels */
      case 0x3088:    /* CU3088 (CTC, LCS) */
	res->io.range++;
    }

    hd=add_hd_entry(hd_data,__LINE__,0);
    add_res_entry(&hd->res,res);
    hd->vendor.id=MAKE_ID(TAG_SPECIAL,0x6001); /* IBM */
    hd->device.id=MAKE_ID(TAG_SPECIAL,cutype);
    hd->sub_device.id=MAKE_ID(TAG_SPECIAL,devtype);
    hd->detail=free_hd_detail(hd->detail);
    hd->detail=new_mem(sizeof *hd->detail);
    hd->detail->ccw.type=hd_detail_ccw;
    hd->detail->ccw.data=new_mem(sizeof(ccw_t));
    hd->detail->ccw.data->cu_model=cumod;
    hd->detail->ccw.data->dev_model=devmod;
    hddb_add_info(hd_data,hd);
  }

}

#endif
