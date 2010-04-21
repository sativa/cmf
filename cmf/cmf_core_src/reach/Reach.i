

// Copyright 2010 by Philipp Kraft
// This file is part of cmf.
//
//   cmf is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 2 of the License, or
//   (at your option) any later version.
//
//   cmf is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with cmf.  If not, see <http://www.gnu.org/licenses/>.
//   
%{
	#include "reach/ReachType.h"
	#include "reach/OpenWaterStorage.h"
	#include "reach/Reach.h"
%}

// Get river model classes
%include "reach/ReachType.h"

%attribute(cmf::river::OpenWaterStorage, real,depth,get_depth,set_depth);

%include "reach/OpenWaterStorage.h"

%attribute(cmf::river::Reach,real,length,get_length);
%attribute2(cmf::river::Reach,cmf::river::IChannel,channel,get_height_function);
%attribute(cmf::river::ReachIterator,cmf::river::Reach,reach,reach);
%attribute(cmf::river::Reach,cmf::river::Reach,downstream,get_downstream);
%attribute(cmf::river::Reach,cmf::river::Reach,root,get_root);
%attribute(cmf::river::Reach,int,upstream_count,upstream_count);
%attribute(cmf::river::Reach,bool,diffusive,get_diffusive,set_diffusive);

%attribute(cmf::river::ReachIterator,double,position,position);

%include "reach/Reach.h"
%extend cmf::river::Reach {
%pythoncode {
    @property
    def upstream(self):
        """Returns a list containing all reaches flowing into self"""
        return [self.get_upstream(i) for i in range(self.upstream_count)]
    def __hash__(self):
        return hash(self.water.node_id)
    def connect_to_cell(self,cell,width,subsurface_connection_type=None,diffusive=None):
        """ Connects a cell with this reach using Manning's equation for surface runoff and
        a given connection for subsurface interflow 
         - subsurface_connection_type : Any lateral flow connection type
         - width : Boundary width in m
         - diffusive: Determines if a kinematic or diffusive wave is to be used for surface runoff
        """
        assert(subsurface_connection_type is None or issubclass(subsurface_connection_type, lateral_sub_surface_flux))
        if diffusive is None:
            diffusive = self.diffusive
        self.connect_to_surfacewater(cell,width,diffusive)
        r_depth = cell.z - self.Location.z
        distance = cell.position.distanceTo(self.Location)
        if subsurface_connection_type:
            cell.connect_soil_with_node(self,subsurface_connection_type,width,distance,0,r_depth)
        
}}


%extend cmf::river::ReachIterator {
%pythoncode {
    def __iter__(self):
        while self.valid():
            self.next()
            yield (self.reach,self.position)
}}

EXTENT__REPR__(cmf::river::OpenWaterStorage)
EXTENT__REPR__(cmf::river::Reach)
