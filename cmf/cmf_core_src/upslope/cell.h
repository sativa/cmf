

// Copyright 2010 by Philipp Kraft
// This file is part of cmf.
//
//   cmf is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
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
#ifndef cell_h__
#define cell_h__
#include "../atmosphere/meteorology.h"
#include "../atmosphere/precipitation.h"
#include "../geometry/geometry.h"
#include "../water/flux_connection.h"
#include "vegetation/StructVegetation.h"
#include "../water/WaterStorage.h"
#include "../math/statevariable.h"
#include "Soil/RetentionCurve.h"
#include "SoilLayer.h"
#include "layer_list.h"
#include "../cmfmemory.h"
#include <map>
#include <vector>
#include <set>
namespace cmf {
	class project;
	namespace river {
		class OpenWaterStorage;	
		class Reach;
	}
    /// Contains the classes to describe the discretization of the soil continuum
	/// @todo: Get Id in constructor for better naming of bounday conditions
	namespace upslope {
		class Topology;
		class Cell;
		namespace ET {
			class RootUptakeStressFunction;
		}
		typedef void (*connectorfunction)(cmf::upslope::Cell&,cmf::upslope::Cell&,ptrdiff_t);
		typedef void (*internal_connector)(cmf::upslope::Cell&);
		typedef std::shared_ptr<SoilLayer> layer_ptr;
		class SurfaceWater;
		typedef std::shared_ptr<SurfaceWater> surfacewater_ptr;

		/// A helper class to connect cells with flux_connection objects. This is generated by flux_connection classes, intended to connect cells 
		class CellConnector
		{
		private:
			connectorfunction m_connector;
			CellConnector():m_connector(0) {}
		public:
			CellConnector(connectorfunction connector) : m_connector(connector)			{	}
			void operator()(cmf::upslope::Cell& cell1,cmf::upslope::Cell& cell2,ptrdiff_t start_at_layer=0) const	{
				connect(cell1,cell2,start_at_layer);
			}
			void connect(cmf::upslope::Cell& cell1,cmf::upslope::Cell& cell2,ptrdiff_t start_at_layer=0) const {
				m_connector(cell1,cell2,start_at_layer);
			}
		};


		/// This class is the basic landscape object. It is the owner of water storages, and the upper and lower boundary conditions 
		/// of the system (rainfall, atmospheric vapor, deep groundwater)
		class Cell : public cmf::math::StateVariableOwner {
			Cell(const Cell& cpy);
			friend class project;
			/// @name Location
			//@{
			std::unique_ptr<Topology> m_topo;
			static ptrdiff_t cell_count;
		public:
			cmf::upslope::Topology& get_topology();
#ifndef SWIG
			operator cmf::upslope::Topology&() {return get_topology();}
#endif
			double x,y,z;
			/// Returns the location of the cell
			cmf::geometry::point get_position() const { return cmf::geometry::point(x,y,z); }
		private:
			double m_Area;
			mutable real m_SatDepth;
		public:
			/// Returns the area of the cell
			double get_area() const { return m_Area; }
			/// Converts a volume in m3 in mm for the cell area
			double m3_to_mm(double volume) const { return volume/m_Area * 1e3;}
			double mm_to_m3(double depth) const { return depth * m_Area * 1e-3;}
			/// @name Saturation
			//@{
			/// Marks the saturated depth as unvalid. This is done automatically, when the state of a layer changes
			void InvalidateSatDepth() const {m_SatDepth=-1e20;}
			/// Returns the potential \f$\Psi_{total}\f$ of the deepest unsaturated layer as distance from the surface.
			///
			/// This function is wrapped as the property `saturated_depth` in Python
			real get_saturated_depth() const;
			/// Sets the potential \f$\Psi_{total}\f$ of each layer as distance from the surface
			///
			/// This function is wrapped as the property `saturated_depth` in Python
			void set_saturated_depth(real depth);
			//@}
		private:
			//@}
			/// @name Flux nodes of the cell
			//@{
			typedef std::vector<cmf::water::WaterStorage::ptr> storage_vector;
			typedef std::unique_ptr<cmf::atmosphere::Meteorology> meteo_pointer;
			storage_vector m_storages;

			typedef std::map<std::string, cmf::water::flux_node::ptr> nodemap;
			nodemap m_cellboundaries;
			cmf::water::WaterStorage::ptr
				m_Canopy,
				m_Snow;
			cmf::upslope::surfacewater_ptr m_SurfaceWaterStorage;
			cmf::water::flux_node::ptr m_SurfaceWater;
			cmf::atmosphere::RainSource::ptr m_rainfall;
			cmf::water::flux_node::ptr m_Evaporation, m_Transpiration;
			cmf::atmosphere::aerodynamic_resistance::ptr m_aerodynamic_resistance;

			cmf::project & m_project;
			meteo_pointer m_meteo;
			
			cmf::bytestring m_WKB;

		public:
			/// The vegetation object of the cell
			cmf::upslope::vegetation::Vegetation vegetation;
			/// Returns the meteorological data source
			cmf::atmosphere::Meteorology& get_meteorology() const 
			{
				return *m_meteo;
			}
			/// Sets the method to calculate aerodynamic resistance against turbulent sensible heat fluxes
			void set_aerodynamic_resistance(cmf::atmosphere::aerodynamic_resistance::ptr Ra) {
				m_aerodynamic_resistance = Ra;
			}
			/// Sets a meteorological data source
			void set_meteorology(const cmf::atmosphere::Meteorology& new_meteo)
			{
				m_meteo.reset(new_meteo.copy());
			}
			/// Sets the weather for this cell. Connectivity to a meteorological station is lost.
			void set_weather(const cmf::atmosphere::Weather& weather)
			{
				cmf::atmosphere::ConstantMeteorology meteo(weather);
				m_meteo.reset(meteo.copy());
			}
			/// Exchanges a timeseries of rainfall with a constant flux
			void set_rainfall(double rainfall);
			/// Returns the current rainfall flux in m3/day
			double get_rainfall(cmf::math::Time t) const {return m_rainfall->get_intensity(t) * 1e-3 * m_Area;}
			/// Changes the current source of rainfall
			void set_rain_source(cmf::atmosphere::RainSource::ptr new_source);
			
			/// Returns the current source for rainfall
			cmf::atmosphere::RainSource::ptr get_rain_source() {
				return m_rainfall;
			}
			/// Uses the given WaterStressFunction for all stressedET like connections to the transpiration target
			void set_uptakestress(const cmf::upslope::ET::RootUptakeStressFunction& stressfunction);
			/*
			/// Experimental feature: Gets a cell owned boundary node, eg. groundwater, or Neumannboundary
			/// This should replace get_evaporation, get_transpiration etc.
			cmf::water::flux_node::ptr get_boundarynode(std::string name);
			/// Experimental feature: Adds a cell owned boundary node, eg. groundwater
			/// This should replace get_evaporation, get_transpiration etc.
			///
			/// @param z: Vertical displacement of the boundary to the cell position
			cmf::water::DirichletBoundary::ptr add_dirichletboundary(std::string name,real z);
			/// Experimental feature: Adds a cell owned boundary node, eg. groundwater
			/// This should replace get_evaporation, get_transpiration etc.
			///
			/// @param x,y,z: Displacement of the boundary to the cell position
			cmf::water::DirichletBoundary::ptr add_dirichletboundary(std::string name,real x,real y,real z);
			/// Experimental feature: Adds a cell owned boundary node, eg. groundwater
			/// This should replace get_evaporation, get_transpiration etc.
			///
			/// @param target: Target water storage of the Neumann boundary condition
			cmf::water::NeumannBoundary::ptr add_neumannboundary(std::string name,cmf::water::WaterStorage::ptr target);
			/// Experimental feature: Adds a cell owned boundary node, eg. irrigation
			/// This should replace get_evaporation, get_transpiration etc.
			///
			/// @param z: Vertical displacement of the boundary to the cell position
			cmf::water::flux_node::ptr add_fluxnode(std::string name,real z);
			/// Experimental feature: Adds a cell owned boundary node, eg. groundwater
			/// This should replace get_evaporation, get_transpiration etc.
			///
			/// @param z: Vertical displacement of the boundary to the cell position
			cmf::water::flux_node::ptr add_fluxnode(std::string name,real x,real y, real z);
			*/



			/// Returns the end point of all evaporation of this cell (a cmf::water::flux_node)
 			cmf::water::flux_node::ptr get_evaporation();
 			/// Returns the end point of all transpiration of this cell (a cmf::water::flux_node)
 			cmf::water::flux_node::ptr get_transpiration();

			/// returns the surface water of this cell. This is either a flux node or a cmf::upslope::SurfaceWater
			cmf::water::flux_node::ptr get_surfacewater();
			
			/// Makes the surfacewater of this cell a cmf::upslope::SurfaceWater storage.
			surfacewater_ptr surfacewater_as_storage();

			/// Adds a new storage to the cell
			///
			/// @param Name The name of the storage
			/// @param storage_role A shortcut to describe the functional role of the storage new storage. Possible Values:
			/// - 'C' denotes a canopy storage
			/// - 'S' denotes a snow storage
			/// - any other value denotes a storage with an undefined function
			/// @param isopenwater If true, an open water storage with a cmf::river::Prism height function is created
			cmf::water::WaterStorage::ptr add_storage(std::string Name,char storage_role='N',  bool isopenwater=false);

			/// Bounds an existing storage to the cell
			ptrdiff_t add_storage(cmf::water::WaterStorage::ptr storage) {
				m_storages.push_back(storage);
				return m_storages.size();
			}

			void remove_storage(cmf::water::WaterStorage::ptr storage);
			size_t storage_count() const
			{
				return size_t(m_storages.size());
			}
			cmf::water::WaterStorage::ptr get_storage(ptrdiff_t index) const;
			cmf::water::WaterStorage::ptr get_canopy() const;
			cmf::water::WaterStorage::ptr get_snow() const;
			real snow_coverage() const;
			real albedo() const;
			/// Returns the coverage of the surface water.
			///
			/// The covered fraction (0..1) is simply modelled as a piecewise linear
			/// function of the surface water depth. If the depth is above the
			/// aggregate height, the coverage is 1, below it is given as
			/// \f[ c = \frac{h_{water}}{\Delta h_{surface}}\f]
			/// with c the coverage, \f$h_{water}\f$ the depth of the surface water and
			/// \f$\Delta h_{surface}\f$ the amplitude of the surface roughness
			real surface_water_coverage() const;
			/// Calculates the surface heat balance
			/// @param t Time step
			real heat_flux(cmf::math::Time t) const;
			real Tground;

			/// Return the fraction of wet leaves in the canopy if a canopy water storage exists.
			/// If no canopy storage is present, it returns 0.0 (=empty). 
			/// The fraction of wet leaves are calculated as the linear filling of the canopy storage.
			real leave_wetness() const;
			
			ptrdiff_t Id;
			cmf::bytestring get_WKB() const {
				return m_WKB;
			}
			void set_WKB(cmf::bytestring wkb) {
				m_WKB = wkb;
			}
			cmf::project& get_project() const
			{
				return m_project;
			}

			/// Returns the current meteorological conditions of the cell at time t.
			cmf::atmosphere::Weather get_weather(cmf::math::Time t) const;

		public:
			//@}
			///@name Layers
			//@{
		private:
			layer_list m_Layers;
		public:
			/// Returns the number of layers of the cell
			size_t layer_count() const
			{
				return size_t(m_Layers.size());
			}
			/// Returns the layer at position ndx.
			///
			/// From python this function is masked as a sequence:
			/// @code{.py}
			/// l = cell.layers[ndx]
			/// @endcode
			cmf::upslope::SoilLayer::ptr get_layer(ptrdiff_t ndx) const;

			/// Returns the list of layers.
			///
			/// From python this function is masked as a property:
			/// @code{.py}
			/// layers = cell.layers
			/// @endcode
			const layer_list& get_layers() const {return m_Layers;}

			/// Adds a layer to the cell. Layers are created using this function
			///
			/// @returns the new layer
			/// @param lowerboundary The maximum depth of the layer in m. If lowerboundary is smaller or equal than the lowerboundary of thelowest layer, an error is raised
			/// @param r_curve A retention curve. [See here](/wiki/CmfTutRetentioncurve) for a discussion on retention curves in cmf.
			/// @param saturateddepth The initial potential of the new layer in m below surface. Default = 10m (=quite dry)
			cmf::upslope::SoilLayer::ptr add_layer(real lowerboundary,const cmf::upslope::RetentionCurve& r_curve,real saturateddepth=10);

			/// Adds a rather conceptual layer to the cell. Use this version for conceptual models. The retention curve resambles an empty bucket
			cmf::upslope::SoilLayer::ptr add_layer(real lowerboundary);
			/// Remove the lowest layer from this cell
			void remove_last_layer();
			/// Removes all layers from this cell
			void remove_layers();
			/// Returns the lower boundary of the lowest layer in m
			double get_soildepth() const;



			virtual ~Cell();
			//@}


			Cell(double x,double y,double z,double area,cmf::project & _project);
			std::string to_string() const;
			//@}
			cmf::math::StateVariableList get_states();
		};

		
	}
	
}
#endif // cell_h__
