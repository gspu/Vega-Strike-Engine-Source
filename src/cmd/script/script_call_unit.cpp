/* 
 * Vega Strike
 * Copyright (C) 2001-2002 Daniel Horn
 * 
 * http://vegastrike.sourceforge.net/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
  xml Mission Scripting written by Alexander Rawass <alexannika@users.sourceforge.net>
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#ifndef WIN32
// this file isn't available on my system (all win32 machines?) i dun even know what it has or if we need it as I can compile without it
#include <unistd.h>
#endif
#include "cmd/unit_factory.h"
#include <expat.h>
#include "xml_support.h"

#include "vegastrike.h"
#include "cmd/collection.h"
#include "cmd/iterator.h"
#include "cmd/planet.h"
#include "cmd/ai/order.h"
#include "cmd/ai/aggressive.h"
#include "cmd/ai/missionscript.h"
#include "gfx/aux_texture.h"
#include "mission.h"
#include "easydom.h"
#include "msgcenter.h"
#include "flightgroup.h"
#include "vs_globals.h"
#include "config_xml.h"
#include "gfx/cockpit.h"
#include "cmd/images.h"
#include "savegame.h"
#include "cmd/nebula.h"
#include "hashtable.h"
#include "gfx/animation.h"
#include "flightgroup.h"
#include "cmd/unit_factory.h"
#include "cmd/asteroid.h"
//#include "vegastrike.h"
extern vector <char *> ParseDestinations (const string &value);

/* *********************************************************** */

extern Unit& GetUnitMasterPartList ();

#if 0
NEVER NEVER NEVER use Unit* to save a unit across frames
extern Unit *player_unit;
BAD BAD BAD

Better:
extern UnitContainer player_unit;

Best:
_Universe.AccessCockpit()->GetParent();
#endif

static Unit * getIthUnit (un_iter uiter, int i);

varInst *Mission::call_unit(missionNode *node,int mode){
#ifdef ORDERDEBUG
  fprintf (stderr,"callun%x",this);
  fflush (stderr);
#endif

  varInst *viret=NULL;

  trace(node,mode);

  if(mode==SCRIPT_PARSE){
    string cmd=node->attr_value("name");
    node->script.method_id=module_unit_map[cmd];
  }

   callback_module_unit_type method_id=(callback_module_unit_type) node->script.method_id;

  if(method_id==CMT_UNIT_getUnit){
    missionNode *nr_node=getArgument(node,mode,0);
    int unit_nr=doIntVar(nr_node,mode);
    Unit *my_unit=NULL;

    if(mode==SCRIPT_RUN){
      StarSystem *ssystem=_Universe.scriptStarSystem();
      //UnitCollection::UnitIterator *uiter=unitlist->createIterator();
      un_iter uiter=ssystem->getUnitList().createIterator();
      my_unit=getIthUnit (uiter,unit_nr);
    }

    viret=newVarInst(VI_TEMP);
    viret->type=VAR_OBJECT;
    viret->objectname="unit";
    
    viret->object=(void *)my_unit;

    debug(3,node,mode,"unit getUnit: ");
    printVarInst(3,viret);

    return viret;
  }
  else if(method_id==CMT_UNIT_getPlayer){

    Unit *my_unit=NULL;

    if(mode==SCRIPT_RUN){
      my_unit=_Universe.AccessCockpit()->GetParent();;
    }

    viret=newVarInst(VI_TEMP);
    viret->type=VAR_OBJECT;
    viret->objectname="unit";
    
    viret->object=(void *)my_unit;

    debug(3,node,mode,"unit getUnit: ");
    printVarInst(3,viret);

    return viret;

  }
  else if(method_id==CMT_UNIT_getPlayerX){
    int which= getIntArg(node,mode,0);
    Unit *my_unit=NULL;

    if(mode==SCRIPT_RUN){
      int j=0;
      for (unsigned int i=0;i<_Universe.numPlayers();i++) {
	Unit * un;
	if (NULL!=(un=_Universe.AccessCockpit(i)->GetParent())) {
	  if (j==which) {
	    my_unit=un;
	    break;
	  }
	  j++;
	}
      }
    }

    viret=newVarInst(VI_TEMP);
    viret->type=VAR_OBJECT;
    viret->objectname="unit";
    
    viret->object=(void *)my_unit;

    debug(3,node,mode,"unit getUnit: ");
    printVarInst(3,viret);

    return viret;

  }
  else if(method_id==CMT_UNIT_launch||method_id==CMT_UNIT_launchNebula||method_id==CMT_UNIT_launchPlanet||method_id==CMT_UNIT_launchJumppoint){
    missionNode *name_node=getArgument(node,mode,0);
    varInst *name_vi=checkObjectExpr(name_node,mode);
    
    missionNode *faction_node=getArgument(node,mode,1);
    varInst *faction_vi=checkObjectExpr(faction_node,mode);
    
    missionNode *type_node=getArgument(node,mode,2);
    varInst *type_vi=checkObjectExpr(type_node,mode);
    
    missionNode *ai_node=getArgument(node,mode,3);
    varInst *ai_vi=checkObjectExpr(ai_node,mode);
    
    missionNode *nr_node=getArgument(node,mode,4);
    int nr_of_ships=checkIntExpr(nr_node,mode);

    missionNode *nrw_node=getArgument(node,mode,5);
    int nr_of_waves=checkIntExpr(nrw_node,mode);
    varInst *destination_vi;
    string destinations;
    string logo_tex;
    string logo_alp;
    if (method_id==CMT_UNIT_launchJumppoint) {
      missionNode *destination_node=getArgument(node,mode,6);
      destination_vi=checkObjectExpr(faction_node,mode);
      if (mode==SCRIPT_RUN) {
	destinations = *((string*)destination_vi->object);
      }
    }
    missionNode *pos_node[3];
    QVector pos (getFloatArg (node,mode,6),
		getFloatArg (node,mode,7),
		getFloatArg (node,mode,8));
    if (node->subnodes.size()>9) {
      logo_tex=getStringArgument (node,mode,9);
      if (node->subnodes.size()>10) {
	logo_alp=getStringArgument (node,mode,10);
      }
    }

	Unit * my_unit=NULL;
    if(mode==SCRIPT_RUN){
      clsptr clstyp=UNITPTR;
      if (method_id==CMT_UNIT_launchJumppoint||method_id==CMT_UNIT_launchPlanet) {
	clstyp=PLANETPTR;
      }else if (method_id==CMT_UNIT_launchNebula){
	clstyp=NEBULAPTR;
      }
      string name_string=*((string *)name_vi->object);
      string faction_string=*((string *)faction_vi->object);
      string type_string=*((string *)type_vi->object);
      string ai_string=*((string *)ai_vi->object);
      CreateFlightgroup cf;
      cf.fg = GameFlightgroup::newGameFlightgroup (name_string,
					   type_string,
					   faction_string,
					   ai_string,
					   nr_of_ships,
					   nr_of_waves,
					   logo_tex,
					   logo_alp,
					   this);

      //      cf.fg->ainame=ai_string;
      cf.unittype=CreateFlightgroup::UNIT;
      cf.terrain_nr=-1;
      cf.waves=nr_of_waves;
      cf.nr_ships=nr_of_ships;
      cf.fg->pos=pos;
      for(int i=0;i<3;i++){
	cf.rot[i]=0.0;
      }
#ifdef ORDERDEBUG
  fprintf (stderr,"cunl%x",this);
  fflush (stderr);
#endif
	  Unit *tmp= call_unit_launch(&cf,clstyp,destinations);
	  number_of_ships+=nr_of_ships;
	  if (!my_unit) {
		my_unit=tmp;
	  }
#ifdef ORDERDEBUG
	  fprintf (stderr,"ecun");
	  fflush (stderr);
#endif
	  
    }

    deleteVarInst(name_vi);
    deleteVarInst(faction_vi);
    deleteVarInst(type_vi);
    deleteVarInst(ai_vi);
    viret=newVarInst(VI_TEMP);
    viret->type=VAR_OBJECT;
    viret->objectname="unit";

    viret->object=(void *)my_unit;

    debug(3,node,mode,"unit getUnit: ");

    return viret;
  }   
  else if(method_id==CMT_UNIT_getRandCargo){
    int quantity=getIntArg(node,mode,0);
    string category;
    if (node->subnodes.size()>1) {
      category = getStringArgument(node,mode,1);
    }
    Unit *my_unit=NULL;
    varInst *vireturn=NULL;
    vireturn=call_olist_new(node,mode);
    if(mode==SCRIPT_RUN){
		Cargo *ret=NULL;
		Unit *mpl = &GetUnitMasterPartList();
		unsigned int max=mpl->numCargo();
		if (!category.empty()) {
		  vector <Cargo> cat;
		  mpl->GetCargoCat (category,cat);
		  if (!cat.empty()) {
		    unsigned int i;
		    ret = mpl->GetCargo(cat[rand()%cat.size()].content,i);
		  }
                }else {
		  if (mpl->numCargo()) {
		    for (unsigned int i=0;i<500;i++) {
		      ret = &mpl->GetCargo(rand()%max);  
		      if (ret->content.find("mission")==string::npos) {
			break;
		      }
		    }
		  }else {
		    ret = new Cargo();//mem leak--won't happen
		  }		  
                }
                
		if (ret) {
		  ret->quantity=quantity;
		
		  viret=newVarInst(VI_IN_OBJECT);
		  viret->type=VAR_OBJECT;
		  viret->objectname="string";
		  viret->object=&ret->content;
 		  ((olist_t *)vireturn->object)->push_back(viret);
		  viret=newVarInst(VI_IN_OBJECT);
		  viret->type=VAR_OBJECT;
		  viret->objectname="string";
		  viret->object=&ret->category;
		  ((olist_t *)vireturn->object)->push_back(viret);
		  viret=newVarInst(VI_IN_OBJECT);
		  viret->type=VAR_FLOAT;
		  viret->float_val=ret->price;
		  ((olist_t *)vireturn->object)->push_back(viret);
		  viret=newVarInst(VI_IN_OBJECT);
		  viret->type=VAR_INT;
		  viret->int_val=quantity;
		  ((olist_t *)vireturn->object)->push_back(viret);
		  viret=newVarInst(VI_IN_OBJECT);
		  viret->type=VAR_FLOAT;
		  viret->float_val=ret->mass;
		  ((olist_t *)vireturn->object)->push_back(viret);
		  viret=newVarInst(VI_IN_OBJECT);
		  viret->type=VAR_FLOAT;
		  viret->float_val=ret->volume;
		  ((olist_t *)vireturn->object)->push_back(viret);				
		  
		}
    }
    debug(3,node,mode,"unit getRandCargo: ");
    printVarInst(3,vireturn);

    return vireturn;

  }
  else{
    varInst *ovi=getObjectArg(node,mode);
    Unit *my_unit=getUnitObject(node,mode,ovi);

      debug(3,node,mode,"unit object: ");
      printVarInst(3,ovi);

      if(method_id==CMT_UNIT_getContainer){
	UnitContainer *cont=NULL;
	if(mode==SCRIPT_RUN){
	  cont=new UnitContainer(my_unit);
	  //	  printf("new container %x\n",cont);
	}

 	viret=newVarInst(VI_TEMP);
	viret->type=VAR_OBJECT;
	viret->objectname="unitContainer";
	viret->object=cont;
     }
      else if(method_id==CMT_UNIT_getUnitFromContainer){
	Unit *ret=NULL;
	if(mode==SCRIPT_RUN){
	  ret=((UnitContainer *)my_unit)->GetUnit();
	  //	  printf("ret from container: %x\n",ret);
	}
 	viret=newVarInst(VI_TEMP);
	viret->type=VAR_OBJECT;
	viret->objectname="unit";
	viret->object=ret;
      }
      else if(method_id==CMT_UNIT_deleteContainer){
	UnitContainer *cont=(UnitContainer *)my_unit;
	if(mode==SCRIPT_RUN){
	  delete cont;
	}
 	viret=newVarInst(VI_TEMP);
	viret->type=VAR_VOID;
      }
    else if(method_id==CMT_UNIT_getPosition){
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="olist";
      if(mode==SCRIPT_RUN){
	QVector pos=my_unit->Position();

	call_vector_into_olist(viret,pos);
      }
    }
    else if(method_id==CMT_UNIT_getFaction){
      if(mode==SCRIPT_RUN){
	//changed 042202
	//    varInst *ovi=getObjectArg(node,mode);
	//    Unit *my_unit=getUnitObject(node,mode,ovi);
		string factionname=FactionUtil::GetFaction(my_unit->faction);
		varInst *str_vi=call_string_new(node,mode,factionname);
		viret=str_vi;
      }else {
		varInst *str_vi=call_string_new(node,mode,"");
		viret =str_vi;
      }
      //      return viret;
    }
    else if(method_id==CMT_UNIT_getVelocity){
      if(mode==SCRIPT_RUN){
	varInst *vec3_vi=call_olist_new(node,mode);

	float vel_mag=my_unit->GetVelocity().Magnitude();
	Vector pos;
	
	printf("VELMAG:%f\n",vel_mag);
	if(fabs (vel_mag)> 10e-6){
	  pos=my_unit->GetVelocity()/vel_mag;
	}
	else{
	  Vector p,q,r;
	  
	  my_unit->GetOrientation(p,q,r);
	  pos=r;
	}

	varInst *pos_vi;

	pos_vi=newVarInst(VI_TEMP);
	pos_vi->type=VAR_FLOAT;
	pos_vi->float_val=pos.i;
	call_olist_push_back(node,mode,vec3_vi,pos_vi);

	pos_vi=newVarInst(VI_TEMP);
	pos_vi->type=VAR_FLOAT;
	pos_vi->float_val=pos.j;
	call_olist_push_back(node,mode,vec3_vi,pos_vi);

	pos_vi=newVarInst(VI_TEMP);
	pos_vi->type=VAR_FLOAT;
	pos_vi->float_val=pos.k;
	call_olist_push_back(node,mode,vec3_vi,pos_vi);

	//	deleteVarInst(vec3_vi);
	viret=vec3_vi;
	//return vec3_vi;
      }
      else{
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_OBJECT;
	viret->objectname="olist";
	//return viret;
      }
    }
    else if(method_id==CMT_UNIT_getTarget){
      Unit *res_unit=NULL;
      if(mode==SCRIPT_RUN){
	res_unit=my_unit->Target();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=res_unit;
    }
    else if(method_id==CMT_UNIT_getName){
      if(mode==SCRIPT_RUN){
	string unit_name;
	unit_name=my_unit->name;

	viret=call_string_new(node,mode,unit_name);
      }
      else{
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_OBJECT;
	viret->objectname="string";
      }
    }
    else if(method_id==CMT_UNIT_setName){
      string s = getStringArgument (node,mode,1);
      if(mode==SCRIPT_RUN){
	my_unit->name=s;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_getThreat){
      Unit *res_unit=NULL;
      if(mode==SCRIPT_RUN){
	res_unit=my_unit->Threat();
	//	printf("threat: %x\n",(int)res_unit);
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=res_unit;
    }
    else if(method_id==CMT_UNIT_setTarget){
      Unit *other_unit=getUnitArg(node,mode,1);
      if(mode==SCRIPT_RUN){
	my_unit->Target(other_unit);
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_equal){
      Unit *other_unit=getUnitArg(node,mode,1);
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=(my_unit==other_unit);//doesn't dereference anything
    }
    else if(method_id==CMT_UNIT_getDistance){
      float dist=0.0;
      Unit * un= getUnitArg(node,mode,1);
      if(mode==SCRIPT_RUN){
	dist = (my_unit->Position()-un->Position()).Magnitude()-my_unit->rSize()-un->rSize();
	//	  dist=my_unit->getMinDis(vec3);
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=dist;
    }
    else if(method_id==CMT_UNIT_getMinDis){
      QVector vec3=getVec3Arg(node,mode,1);

	double dist=0.0;
	if(mode==SCRIPT_RUN){
	  dist = (my_unit->Position()-vec3).Magnitude()-my_unit->rSize();
	  //	  dist=my_unit->getMinDis(vec3);
	}
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_FLOAT;
	viret->float_val=dist;
    }
    else if(method_id==CMT_UNIT_getAngle){
      Unit *other_unit=getUnitArg(node,mode,1);

	float angle=0.0;
	float dist=0.0;
	if(mode==SCRIPT_RUN){
	  Vector p,q,r;
	  QVector vectothem=QVector(other_unit->Position() - my_unit->Position()).Normalize();
	  my_unit->GetOrientation(p,q,r);
	  //angle=my_unit->cosAngleTo(other_unit,dist);
	  angle=acos( vectothem.Dot(r.Cast()) );
	  angle=(angle/PI)*180.0;
	  //printf("angle: %f\n",angle);
	}
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_FLOAT;
	viret->float_val=angle;
    }
    else if(method_id==CMT_UNIT_getAngleToPos){
      QVector other_pos=getVec3Arg(node,mode,1);

	double angle=0.0;
	double dist=0.0;
	if(mode==SCRIPT_RUN){
	  Vector p,q,r;
	  QVector vectothem=QVector(other_pos - my_unit->Position()).Normalize();
	  my_unit->GetOrientation(p,q,r);
	  //angle=my_unit->cosAngleTo(other_unit,dist);
	  angle=acos( vectothem.Dot(r.Cast()) );
	  angle=(angle/PI)*180.0;
	  //printf("angle: %f\n",angle);
	}
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_FLOAT;
	viret->float_val=angle;
    }
    else if(method_id==CMT_UNIT_getFShieldData){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->FShieldData();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_getRShieldData){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->RShieldData();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_getLShieldData){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->LShieldData();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_getBShieldData){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->BShieldData();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_getEnergyData){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->EnergyData();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_getHullData){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->GetHull();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_getRSize){
      float res=0.0;
      if(mode==SCRIPT_RUN){
	res=my_unit->rSize();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if(method_id==CMT_UNIT_isStarShip){
      bool res=false;
      if(mode==SCRIPT_RUN){
	res=my_unit->isStarShip();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=res;
    }
    else if(method_id==CMT_UNIT_isPlanet){
      bool res=false;
      if(mode==SCRIPT_RUN){
	res=my_unit->isPlanet();
	if (res) {
	  res = !(((Planet *)my_unit)->hasLights());
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=res;
    }
    else if(method_id==CMT_UNIT_isSignificant){
      bool res=false;
      if(mode==SCRIPT_RUN){
	clsptr typ = my_unit->isUnit();
	string s=  my_unit->getFlightgroup()?my_unit->getFlightgroup()->name:"";
	
	res=(typ==PLANETPTR&&!((Planet *)my_unit)->hasLights())||typ==ASTEROIDPTR||typ==NEBULAPTR||s=="Base";
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=res;
    }
    else if(method_id==CMT_UNIT_isSun){
      bool res=false;
      if(mode==SCRIPT_RUN){
	res=my_unit->isPlanet();
	if (res) {
	  res = ((Planet *)my_unit)->hasLights();
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=res;
    }
    else if(method_id==CMT_UNIT_isJumppoint){
      bool res=false;
      if(mode==SCRIPT_RUN){
	res=my_unit->isJumppoint();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=res;
    }
    else if(method_id==CMT_UNIT_getRelation){
      float res=0.0;
      Unit *other_unit=getUnitArg(node,mode,1);

      if(mode==SCRIPT_RUN){
	res=my_unit->getRelation(other_unit);
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=res;
    }
    else if (method_id==CMT_UNIT_getCredits) {
     viret=newVarInst(VI_TEMP);
     viret->type=VAR_FLOAT;
     viret->float_val=0;
     if (mode==SCRIPT_RUN) {
       Cockpit * tmp;
       if ((tmp=_Universe.isPlayerStarship (my_unit))) {
	 viret->float_val=tmp->credits;
       }
     }
     return viret;

   }
  else if (method_id==CMT_UNIT_addCredits) {
     missionNode *nr_node=getArgument(node,mode,1);
     float credits=doFloatVar(nr_node,mode);
     if (mode==SCRIPT_RUN) {
       Cockpit * tmp;
       if ((tmp=_Universe.isPlayerStarship (my_unit))) {
	 tmp->credits+=credits;
       }
     }
     viret=newVarInst(VI_TEMP);
     viret->type=VAR_VOID;
     return viret;

   }
    else if(method_id==CMT_UNIT_Jump){
      if(mode==SCRIPT_RUN){
	my_unit->ActivateJumpDrive();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_getOrientationP){
#if 0
      //TODO
      if(mode==SCRIPT_RUN){
	res=my_unit->FShieldData();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
      viret->float_val=res;
#endif
    }
    else if(method_id==CMT_UNIT_getOrder){
      Order *my_order=NULL;

      if(mode==SCRIPT_RUN){
	my_order=my_unit->getAIState();
      }

      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="order";
      viret->object=(void *)my_order;
    }
    else if(method_id==CMT_UNIT_correctStarSystem){
      bool ret=false;
      if(mode==SCRIPT_RUN){
	ret = my_unit->InCorrectStarSystem(_Universe.activeStarSystem());
	
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_BOOL;
      viret->bool_val=ret;

    }
    else if(method_id==CMT_UNIT_removeFromGame){

      if(mode==SCRIPT_RUN){
	my_unit->Kill();
      }

      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_getFgName){
      if(mode==SCRIPT_RUN){
	string fgname;
	Flightgroup *fg=my_unit->getFlightgroup();
	if(fg){
	  fgname=fg->name;
	}
	if(fgname.empty()){
	  fgname="-none-";
	}

	varInst *str_vi=call_string_new(node,mode,fgname);
	viret=str_vi;
      }
      else{
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_OBJECT;
	viret->objectname="string";
      }
    }
    else if(method_id==CMT_UNIT_getFgLeader){
      Unit * ret_unit=NULL;
      if(mode==SCRIPT_RUN){
	ret_unit = (my_unit->getFlightgroup()!=NULL)?my_unit->getFlightgroup()->leader.GetUnit():my_unit;
	if (ret_unit==NULL) {
	  ret_unit=my_unit;
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=(void *)ret_unit;
    }
    else if(method_id==CMT_UNIT_setFgLeader){
      Unit * un=getUnitArg (node,mode,1);
      if(mode==SCRIPT_RUN){
	if (my_unit->getFlightgroup()!=NULL) {
	  my_unit->getFlightgroup()->leader.SetUnit(un);
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_getFgDirective){
      string fgdir ("b");

      if(mode==SCRIPT_RUN){
	fgdir = (my_unit->getFlightgroup()!=NULL)?my_unit->getFlightgroup()->directive:string("b");
      }
      viret=call_string_new(node,mode,fgdir);
    }
    else if(method_id==CMT_UNIT_setFgDirective){
      string inp = getStringArgument (node,mode,1);
      if(mode==SCRIPT_RUN){
	if (my_unit->getFlightgroup()!=NULL) {
	  my_unit->getFlightgroup()->directive = inp;
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_getFgSubnumber){
      int num=0;
      if(mode==SCRIPT_RUN){
	num=my_unit->getFgSubnumber();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_INT;
      viret->int_val=num;
    }
    else if(method_id==CMT_UNIT_scanSystem){
      if(mode==SCRIPT_RUN){
	my_unit->scanSystem();
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_scannerNearestEnemy){
      Unit *ret_unit=NULL;
      if(mode==SCRIPT_RUN){
	ret_unit=my_unit->getScanner()->nearest_enemy;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=(void *)ret_unit;
    }
    else if(method_id==CMT_UNIT_scannerNearestFriend){
      Unit *ret_unit=NULL;
      if(mode==SCRIPT_RUN){
	ret_unit=my_unit->getScanner()->nearest_friend;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=(void *)ret_unit;
    }
    else if(method_id==CMT_UNIT_scannerNearestShip){
      Unit *ret_unit=NULL;
      if(mode==SCRIPT_RUN){
	ret_unit=my_unit->getScanner()->nearest_ship;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=(void *)ret_unit;
    }
    else if(method_id==CMT_UNIT_scannerLeader){
      Unit *ret_unit=NULL;
      if(mode==SCRIPT_RUN){
	ret_unit=my_unit->getScanner()->leader;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      viret->object=(void *)ret_unit;
    }
    else if(method_id==CMT_UNIT_scannerNearestEnemyDist){
      float ret=9999999.0;
      if(mode==SCRIPT_RUN){
	ret=my_unit->getScanner()->nearest_enemy_dist;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=ret;
    }
    else if(method_id==CMT_UNIT_scannerNearestFriendDist){
      float ret=9999999.0;
      if(mode==SCRIPT_RUN){
	ret=my_unit->getScanner()->nearest_friend_dist;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=ret;
    }
    else if(method_id==CMT_UNIT_scannerNearestShipDist){
      float ret=9999999.0;
      if(mode==SCRIPT_RUN){
	ret=my_unit->getScanner()->nearest_ship_dist;
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=ret;
    }
    else if(method_id==CMT_UNIT_getTurret){
      missionNode *nr_node=getArgument(node,mode,1);
      int unit_nr=doIntVar(nr_node,mode);
      Unit *turret_unit=NULL;
      
      if(mode==SCRIPT_RUN){
	un_iter uiter=my_unit->getSubUnits();
	turret_unit=getIthUnit(uiter,unit_nr);
      }
      
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname="unit";
      
      viret->object=(void *)turret_unit;
      
      debug(3,node,mode,"unit getUnit: ");
      printVarInst(3,viret);
    }
    else if(method_id==CMT_UNIT_getFgId){
      if(mode==SCRIPT_RUN){
	
		string fgname=my_unit->getFgID();

		if(fgname.empty()){
		  fgname="-unknown";
		}


		varInst *str_vi=call_string_new(node,mode,fgname);

		viret=str_vi;
		//return str_vi;
      }

      else{
	viret=newVarInst(VI_TEMP);
	viret->type=VAR_OBJECT;
	viret->objectname="string";
	//return vi;
      }
    }
    else if(method_id==CMT_UNIT_removeCargo){
      
      string s = getStringArgument (node,mode,1);

      int quantity= getIntArg(node,mode,2);
      bool erasezero= getBoolArg(node,mode,3);
      if(mode==SCRIPT_RUN){
	int numret=0;
	unsigned int index;
	if (my_unit->GetCargo(s,index)) {
	  quantity = my_unit->RemoveCargo (index,quantity,erasezero);
	}else {
	  quantity=0;
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_INT;
      viret->int_val = quantity;
    }
    else if(method_id==CMT_UNIT_addCargo){
      Cargo carg;
      carg.content = getStringArgument (node,mode,1);
      carg.category = getStringArgument (node,mode,2);
      carg.price= getFloatArg(node,mode,3);
      carg.quantity= getIntArg(node,mode,4);
      carg.mass =getFloatArg(node,mode,5);
      carg.volume =getFloatArg(node,mode,6);
      if(mode==SCRIPT_RUN){
	int i;
	for (i=carg.quantity;i>0&&!my_unit->CanAddCargo(carg);i--) {
	  carg.quantity=i;
	}
	if (i>0) {
	  carg.quantity=i;
	  my_unit->AddCargo(carg);
	}else {
	  carg.quantity=0;
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_INT;
      viret->int_val = carg.quantity;
    }
    else if(method_id==CMT_UNIT_setPosition){
      double x= getFloatArg(node,mode,1);
      double y= getFloatArg(node,mode,2);
      double z= getFloatArg(node,mode,3);
      if(mode==SCRIPT_RUN){
	my_unit->SetCurPosition(QVector(x,y,z));
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if (method_id==CMT_UNIT_upgrade) {
      string file = getStringArgument (node,mode,1);
      double percentage=0;
      bool force=true;
      bool loop_through_mounts=false;
      int mountoffset = getIntArg(node,mode,2);      
      int subunitoffset = getIntArg(node,mode,3);      
      unsigned int siz = node->subnodes.size();
      if (siz>4) {
        force = getBoolArg(node,mode,4);
	if (siz>5) {
	  loop_through_mounts =getBoolArg (node,mode,5);
	}
      }
      if (mode==SCRIPT_RUN) {
	printf ("upgrading %s %s %d %d %s\n",my_unit->name.c_str(),file.c_str(),mountoffset, subunitoffset,loop_through_mounts?"true":"false");
        fflush (stdout);
	percentage=my_unit->Upgrade (file,mountoffset,subunitoffset,force,loop_through_mounts);
	my_unit->SetTurretAI();
      }

      viret=newVarInst(VI_TEMP);
      viret->type=VAR_FLOAT;
      viret->float_val=percentage;
      
    }
    else if(method_id==CMT_UNIT_incrementCargo){
      double percentagechange= getFloatArg(node,mode,1);
      int quantity= getIntArg(node,mode,2);
      if(mode==SCRIPT_RUN){
	if (my_unit->numCargo()>0) {
	  unsigned int index;
	  index = rand()%my_unit->numCargo();
	  Cargo c(my_unit->GetCargo(index));	  
	  c.quantity=quantity;
	  if (my_unit->CanAddCargo(c)) {
	    my_unit->AddCargo(c);
	    my_unit->GetCargo(index).price*=percentagechange;
	  }
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_decrementCargo){
      float percentagechange;
      percentagechange= getFloatArg(node,mode,1);
      if(mode==SCRIPT_RUN){
	if (my_unit->numCargo()>0) {
	  unsigned int index;
	  index = rand()%my_unit->numCargo();
	  if (my_unit->RemoveCargo(index,1,false)) {
	    my_unit->GetCargo(index).price*=percentagechange;
	  }
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_getSaveData){//<>
      std::string magic_num;
      void * my_obj=NULL;
      magic_num= getStringArgument(node,mode,1);
      if(mode==SCRIPT_RUN){
	Cockpit * tmp;
	if ((tmp=_Universe.isPlayerStarship (my_unit))) {
	  my_obj=(void *)&tmp->savegame->getMissionData (magic_num);	  
	}
      }
      viret=newVarInst(VI_TEMP);
      viret->type=VAR_OBJECT;
      viret->objectname = "olist";
      viret->object = my_obj;
    }
    else if(method_id==CMT_UNIT_toxml){
      if(mode==SCRIPT_RUN){
	call_unit_toxml(node,mode,ovi);
      }
      viret =newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
      //return viret;
    }
    else if(method_id==CMT_UNIT_switchFg){
      string arg = getStringArgument(node,mode,1);
      if(mode==SCRIPT_RUN){
	string type= my_unit->name;
	int nr_waves_left=0;
	int nr_ships=1;
	string order("default");
	Flightgroup *fg = my_unit->getFlightgroup();
	if (fg) {
	  type = fg->type;
	  nr_waves_left = fg->nr_waves_left;
	  nr_ships =1;
	  fg->Decrement(my_unit);
	  order = fg->ainame;
	}
	fg = GameFlightgroup::newGameFlightgroup (arg,type,FactionUtil::GetFaction(my_unit->faction),order,nr_ships,nr_waves_left,"","",this);
	my_unit->SetFg (fg,fg->nr_ships_left-1);
      }
      viret =newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_frameOfReference){
      Unit * other_unit = getUnitArg (node,mode,1);
      if (mode==SCRIPT_RUN) {
	my_unit->VelocityReference (other_unit);
      }
      viret = newVarInst (VI_TEMP);
      viret->type=VAR_VOID;
    }
    else if(method_id==CMT_UNIT_communicateTo){
      Unit *other_unit=getUnitArg(node,mode,1);
      float mood =getFloatArg(node,mode,2);
      unsigned char sex=0;
      if(mode==SCRIPT_RUN){
	Cockpit * tmp;
	if ((tmp=_Universe.isPlayerStarship (my_unit))) {
	  Order * ord=other_unit->getAIState();
	  if (ord) {
	    Animation * ani= ord->getCommFace (mood,sex);
	    if (NULL!=ani) {
	      tmp->SetCommAnimation (ani);
	    }
	  }
	}
      }
      viret =newVarInst(VI_TEMP);
      viret->type=VAR_INT;
      viret->int_val = sex;
    }

    else if(method_id==CMT_UNIT_commAnimation){
      string anim =getStringArgument (node,mode,1);
      if(mode==SCRIPT_RUN){
	Cockpit * tmp;
	if ((tmp=_Universe.isPlayerStarship (my_unit))) {
	  Hashtable <std::string, Animation, char [63]> AniHashTable;
	  Animation * ani= AniHashTable.Get(anim);
	  if (NULL==ani) {
	    ani = new Animation (anim.c_str());
	    AniHashTable.Put(anim,ani);
	  }
	  tmp->SetCommAnimation (ani);
	}
      }
      viret =newVarInst(VI_TEMP);
      viret->type=VAR_VOID;
      //return viret;
    }

    else{
      fatalError(node,mode,"no such method "+node->script.name);
      assert(0);
    }
#ifdef ORDERDEBUG
  fprintf (stderr,"callundel%x",ovi);
  fflush (stderr);
#endif

    deleteVarInst(ovi);
#ifdef ORDERDEBUG
  fprintf (stderr,"undel1");
  fflush (stderr);
#endif

    return viret;
  }//else (objects)
#ifdef ORDERDEBUG
  fprintf (stderr,"endcallun%x",this);
  fflush (stderr);
#endif

  return NULL; //never reach
}

Unit *Mission::getUnitObject(missionNode *node,int mode,varInst *ovi){
  Unit *my_object=NULL;

	if(mode==SCRIPT_RUN){
	  my_object=(Unit *)ovi->object;
#if 0
	  if(my_object==NULL){
	    fatalError(node,mode,"unit: no object");
	    assert(0);
	  }
#endif
	}

	return(my_object);
}

// void call_unit_launch(missionNode *node,int mode,string name,string faction,string type,string ainame,int nr_ships,Vector & pos){
extern BLENDFUNC parse_alpha (const char *);

Unit * Mission::call_unit_launch(CreateFlightgroup *fg, int type, const string &destinations){
  //  fprintf (stderr,"calling unit launch with Mission 0x%x Flightgroup 0x%x" ,this, fg);
   int faction_nr=FactionUtil::GetFaction(fg->fg->faction.c_str());
   //   printf("faction nr: %d %s\n",faction_nr,fg->faction.c_str());
   Unit **units= new Unit *[fg->nr_ships];
   int u;
   for(u=0;u<fg->nr_ships;u++){
     Unit * my_unit;
     if (type==PLANETPTR) {
       float radius;
       char * tex = strdup (fg->fg->type.c_str());
       char * bsrc = strdup (fg->fg->type.c_str());
       char * bdst = strdup (fg->fg->type.c_str());
       char * citylights = strdup (fg->fg->type.c_str());
       tex[0]='\0';
       bsrc[0]='\0';//have at least 1 char
       bdst[0]='\0';
       citylights[0]='\0';
       int ret =sscanf (fg->fg->type.c_str(),"%f %s %s %s",&radius,tex,bsrc,bdst);
       GFXMaterial mat;
       GFXGetMaterial (0,mat);
      
       BLENDFUNC s = ONE;
       BLENDFUNC d = ZERO;
       if (bdst[0]!='\0') 
	 d=parse_alpha (bdst);
       if (bsrc[0]!='\0') 
	 s=parse_alpha (bsrc);
       my_unit = GameUnitFactory::createPlanet (QVector(0,0,0),QVector(0,0,0),0,Vector(0,0,0), 0,0,radius,tex,s,d, ParseDestinations(destinations),QVector(0,0,0),NULL,mat,vector<GFXLightLocal>(),faction_nr,tex);

       free (bsrc);
       free (bdst);
       free (tex);
       free (citylights);
     }else if (type==NEBULAPTR) {
       my_unit=GameUnitFactory::createNebula (fg->fg->type.c_str(),false,faction_nr,fg->fg,u);
     } else if (type==ASTEROIDPTR) {
       my_unit=GameUnitFactory::createAsteroid(fg->fg->type.c_str(),faction_nr,fg->fg,u);
     }else {
       my_unit=GameUnitFactory::createUnit(fg->fg->type.c_str(),false,faction_nr,string(""),fg->fg,u);
     }
     units[u]=my_unit;
   }

   float fg_radius=units[0]->rSize();
   Unit *my_unit;
   for(u=0;u<fg->nr_ships;u++){
     my_unit=units[u];

     QVector pox;

     pox.i=fg->fg->pos.i+u*fg_radius*3;
     pox.j=fg->fg->pos.j+u*fg_radius*3;
     pox.k=fg->fg->pos.k+u*fg_radius*3;

     my_unit->SetPosAndCumPos(pox);
     if (type==ASTEROIDPTR||type==NEBULAPTR) {
       my_unit->PrimeOrders();
     }else {
     my_unit->LoadAIScript (fg->fg->ainame);
#if 0
     if(fg->fg->ainame[0]!='_'){
       string ai_agg=fg->fg->ainame+".agg.xml";
       string ai_int=fg->fg->ainame+".int.xml";

       char ai_agg_c[1024];
       char ai_int_c[1024];
       strncpy(ai_agg_c,ai_agg.c_str(),1023);
       strncpy(ai_int_c,ai_int.c_str(),1023);
       ai_agg_c[1023]=0;
       ai_int_c[1023]=0;
       //      printf("1 - %s  2 - %s\n",ai_agg_c,ai_int_c);

       my_unit->EnqueueAI( new Orders::AggressiveAI (ai_agg_c, ai_int_c));
     }
     else{
	      string modulename=fg->fg->ainame.substr(1);

	      if(fg->fg->orderlist==NULL){
		my_unit->EnqueueAI( new AImissionScript(modulename));
	      }
	      else{
		my_unit->EnqueueAI( new AIOrderList(fg->fg->orderlist));
		printf("LAUNCHING a new orderlist ai\n");
	      }
	      //fighters[a]->SetAI( new AImissionScript(modulename));
     }
#endif
     my_unit->SetTurretAI ();
     }
     //     cout << fg->name << endl;

     _Universe.scriptStarSystem()->AddUnit(my_unit);

     //findNextEnemyTarget(my_unit);
     my_unit->Target(NULL);
   }

   printf ("%s",(fg->fg->faction+string ("launched ")+fg->fg->name+string(":")+fg->fg->type+string("0-")+XMLSupport::tostring(fg->fg->nr_ships)).c_str());
   my_unit= units[0];
   if (!_Universe.isPlayerStarship(fg->fg->leader.GetUnit())) {
	   fg->fg->leader.SetUnit(my_unit);
   }
   delete [] units;
   return my_unit;
}

void Mission::findNextEnemyTarget(Unit *my_unit){
      StarSystem *ssystem=_Universe.scriptStarSystem();

      un_iter uiter(ssystem->getUnitList().createIterator());

      int i=0;
      Unit *unit=uiter.current();
      Unit *target_unit=NULL;
      //      int my_faction=_Universe.GetFaction(my_unit->getFlightgroup()->faction.c_str());
      while(unit!=NULL){
	//	int other_faction=_Universe.GetFaction(unit->getFlightgroup()->faction.c_str());
	if(my_unit->getRelation(unit)<0.0){
	  target_unit=uiter.current();
	  unit=NULL;
	}
	else{
	  //	  printf("relation was: %f %d %d\n",_Universe.GetRelation(my_faction,other_faction),my_faction,other_faction);
	  unit=++(uiter);
	  i++;
	}
      }

      if(target_unit==NULL){
	//
      }
      else{
	my_unit->Target(target_unit);
      }

      return;
}

static Unit * getIthUnit (un_iter uiter, int unit_nr) {
  int i=0;
  Unit * my_unit=NULL;
  Unit *unit=uiter.current();
  while(unit!=NULL){
    if(i==unit_nr){
      my_unit=uiter.current();
      unit=NULL;
    }
    else{
      unit=++(uiter);
      i++;
    }
  }
  return my_unit;
}
void Mission::call_unit_toxml(missionNode *node,int mode,varInst *ovi){
  Unit *my_object=getUnitObject(node,mode,ovi);

  if (my_object) {
   string fgid=my_object->getFgID();
   Flightgroup *fg=my_object->getFlightgroup();

  
   var_out << "<unit fgid=" << qu(fgid) << "/>\n";
  }
}
