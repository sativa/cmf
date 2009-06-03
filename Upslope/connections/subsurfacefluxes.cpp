#include "subsurfacefluxes.h"
#include "../../math/real.h"

/************************************************************************/
/* Precolation and 3D fluxes                                            */
/************************************************************************/
// Richards eq.
const cmf::upslope::CellConnector cmf::upslope::connections::Richards::cell_connector=CellConnector(cmf::upslope::connections::Richards::connect_cells);
void cmf::upslope::connections::Richards::connect_cells( cmf::upslope::Cell & cell1,cmf::upslope::Cell & cell2,int start_at_layer/*=0*/ )
{
	for (int i = start_at_layer; i < (start_at_layer>=0 ? cell1.layer_count() : 0) ; ++i)
	{
		for (int j = start_at_layer; j < (start_at_layer>=0 ? cell2.layer_count() : 0) ; ++j)
		{
			real ca=cell1.get_layer(i).GetFlowCrosssection(cell2.get_layer(j));
			if (ca>0)
			{
				real d=cell1.get_layer(i).Location.distance3DTo(cell2.get_layer(i).Location);
				new Richards(cell1.get_layer(i),cell2.get_layer(j),ca,d);
			}
		}
	}
}

void cmf::upslope::connections::Richards::use_for_cell( cmf::upslope::Cell & cell,bool no_override/*=true*/ )
{
	for (int i = 0; i < cell.layer_count()-1 ; ++i)
	{
		cmf::upslope::SoilWaterStorage & l_upper=cell.get_layer(i), & l_lower=cell.get_layer(i+1);
		real distance=abs((l_upper.UpperBoundary()-l_lower.UpperBoundary())*0.5+(l_upper.LowerBoundary()-l_lower.LowerBoundary())*0.5);
		if (!(no_override && l_upper.Connection(l_lower)))
			new Richards(l_upper,l_lower,cell.get_area(),distance);
	}
}

real cmf::upslope::connections::Richards::calc_q( cmf::math::Time t ) 
{
	// Richards flux
	real
		Psi_t1=sw1->Potential(),
		Psi_t2=m_right->Potential(),
		gradient=(Psi_t1-Psi_t2)/distance,
		//K=gradient<0 && sw2 ? sw2->K() : sw1->K();      
		K=sw2 ? geo_mean(sw1->K(),sw2->K()) : sw1->K();
 	if (abs(K*gradient)>sw1->Ksat()) K=sw1->Ksat();
 	if (sw2 && abs(K*gradient)>sw2->Ksat()) K=sw2->Ksat();
	real r_flow=K*gradient*flow_area;
	if (m_left->is_empty())
		r_flow=minimum(0,r_flow);
	if (m_right->is_empty())
		r_flow=maximum(0,r_flow);
	return r_flow; 
}


// Connection of a flexible size saturated zone
real cmf::upslope::connections::UnsatSatPercolation::calc_q( cmf::math::Time t ) 
{
	Cell & cell=m_sat->cell;
	// Store saturated depth
	real satdepth=m_unsat->LowerBoundary();
	real sat_thick=m_sat->Thickness();
	real max_sat_thick=m_sat->MaximumThickness();
	real gradient=(m_unsat->Potential()-m_sat->Potential())/(m_unsat->Thickness()*0.5);
	// gravitational flux
	real Ku=geo_mean(m_unsat->K(),m_sat->K()) * cell.get_area() * gradient;
	real sat_wb=m_sat->water_balance(t,this)+Ku; //*f_upwelling;
	real f_Exw=1-pow(sat_thick/max_sat_thick,100);
	real Exw = f_Exw * sat_wb*m_unsat->Wetness();
	return Ku + Exw;
}

// SWAT Percolation
real cmf::upslope::connections::SWATPercolation::calc_q( cmf::math::Time t )
{
	real
		capacity=sw1->Capacity(),
		fc_waterhead=pressure_to_waterhead(-33000), // Field capacity suction in m
		fc=sw1->Soil().Wetness(fc_waterhead)*capacity, // Water storage at field capacity
		sw_excess=sw1->State()-fc; // drainable water volume
	if (sw_excess<=0) return 0.0;
	real
		Vol_d=sw1->Capacity()-fc, // drainable pore volume in m3
		Ksat=sw1->Soil().K(1,0.5*(sw1->UpperBoundary()+sw1->LowerBoundary())),
		// Percolation travel time in days = drainable volume[m3]/(get_area[m2] * Conductivity[m/day])
		tt_perc=maximum(sw1->Capacity()-fc,0)/(sw1->cell.get_area() * Ksat),
		w_perc=sw_excess*(1-exp(-1/tt_perc));
	return w_perc*minimum(0,0.9-sw2->Wetness());


}

void cmf::upslope::connections::SWATPercolation::use_for_cell( cmf::upslope::Cell & cell,bool no_override )
{
	for (int i = 0; i < cell.layer_count()-1 ; ++i)
	{
		cmf::upslope::SoilWaterStorage & l_upper=cell.get_layer(i), & l_lower=cell.get_layer(i+1);
		real distance=abs((l_upper.UpperBoundary()-l_lower.UpperBoundary())*0.5+(l_upper.LowerBoundary()-l_lower.LowerBoundary())*0.5);
		if (!(no_override && l_upper.Connection(l_lower)))
			new SWATPercolation(l_upper,l_lower);
	}

}
// unsaturated Darcy
const cmf::upslope::CellConnector cmf::upslope::connections::UnsaturatedDarcy::cell_connector=CellConnector(cmf::upslope::connections::UnsaturatedDarcy::connect_cells);
real cmf::upslope::connections::UnsaturatedDarcy::calc_q( cmf::math::Time t )
{
	real
		Psi1=sw1->cell.z-sw1->UpperBoundary(),
		Psi2=sw2->cell.z-sw2->UpperBoundary(),
		gradient=(Psi1-Psi2)/distance,
		K=gradient>0 ? sw1->K() : sw2->K();
	SoilWaterStorage& target=gradient>0 ? *sw2 : *sw1;
	real f_upwelling=1-2*boltzmann(target.Wetness(),1,0.01);
	if (f_upwelling<0)
	{
		real sat_wb=target.water_balance(t,this);
		return f_upwelling*sat_wb;
	}
	else
	{
		return K*gradient*flow_area*f_upwelling;
	}
	
}

void cmf::upslope::connections::UnsaturatedDarcy::connect_cells(cmf::upslope::Cell & cell1,cmf::upslope::Cell & cell2,int start_at_layer/*=0*/ )
{
	for (int i = start_at_layer; i < (start_at_layer>=0 ? cell1.layer_count() : 0) ; ++i)
	{
		for (int j = start_at_layer; j < (start_at_layer>=0 ? cell2.layer_count() : 0) ; ++j)
		{
			real ca=cell1.get_layer(i).GetFlowCrosssection(cell2.get_layer(j));
			if (ca>0)
			{
				real d=cell1.get_layer(i).Location.distance3DTo(cell2.get_layer(i).Location);
				new UnsaturatedDarcy(cell1.get_layer(i),cell2.get_layer(j),ca,d);
			}
		}
	}
}

void cmf::upslope::connections::UnsaturatedDarcy::use_for_cell( cmf::upslope::Cell & cell,bool no_override/*=true*/ )
{
	for (int i = 0; i < cell.layer_count()-1 ; ++i)
	{
		cmf::upslope::SoilWaterStorage & l_upper=cell.get_layer(i), & l_lower=cell.get_layer(i+1);
		real distance=abs((l_upper.UpperBoundary()-l_lower.UpperBoundary())*0.5+(l_upper.LowerBoundary()-l_lower.LowerBoundary())*0.5);
		if (!(no_override && l_upper.Connection(l_lower)))
			new UnsaturatedDarcy(l_upper,l_lower,cell.get_area(),distance);
	}

}

/************************************************************************/
/* Lateral fluxes                                                       */
/************************************************************************/
// Darcy connection
real cmf::upslope::connections::Darcy::calc_q( cmf::math::Time t )	
{
	// Darcy flux
	real
		flow_thick1=minimum(sw1->Thickness(),sw1->LowerBoundary()-sw1->SaturatedDepth()),
		flow_thick2=sw2 ? minimum(sw2->Thickness(),sw2->LowerBoundary()-sw2->SaturatedDepth()) : flow_thick1;
	if (flow_thick1<0 || flow_thick2<0)
		return 0.0;
	real
		// Gravitational gradient
		Psi_t1=sw1->cell.z - sw1->SaturatedDepth(),
		Psi_t2=sw2 ? sw2->cell.z - sw2->SaturatedDepth() : m_right->Potential(),
		gradient=(Psi_t1-Psi_t2)/distance,
		// Transmissivity
		T1=sw1->Soil().Transmissivity(sw1->LowerBoundary()-flow_thick1,sw1->LowerBoundary(),sw1->Wetness()),
		T2=sw2 ? sw2->Soil().Transmissivity(sw2->LowerBoundary()-flow_thick2,sw2->LowerBoundary(),sw2->Wetness()) : T1,
		T = geo_mean(T1,T2);
	return T*gradient*flow_width;
}

void cmf::upslope::connections::Darcy::connect_cells( cmf::upslope::Cell & cell1,cmf::upslope::Cell & cell2,int start_at_layer/*=0*/ )
{
	real w=cell1.get_topology().flowwidth(cell2);
	if (w>0)
	{
		for (int i = start_at_layer; i < (start_at_layer>=0 ? cell1.layer_count() : 0) ; ++i)	{
			for (int j = start_at_layer; j < (start_at_layer>=0 ? cell2.layer_count() : 0) ; ++j)	{
				real ca=cell1.get_layer(i).GetFlowCrosssection(cell2.get_layer(j));
				if (ca>0)	{
					real d=cell1.get_layer(i).Location.distance3DTo(cell2.get_layer(i).Location);
					new Darcy(cell1.get_layer(i),cell2.get_layer(j),w,d);
				}	}	}
	}
}
const cmf::upslope::CellConnector cmf::upslope::connections::Darcy::cell_connector=CellConnector(cmf::upslope::connections::Darcy::connect_cells);

// Topographic gradient connection
const cmf::upslope::CellConnector cmf::upslope::connections::TopographicGradientDarcy::cell_connector=CellConnector(cmf::upslope::connections::TopographicGradientDarcy::connect_cells);
real cmf::upslope::connections::TopographicGradientDarcy::calc_q( cmf::math::Time t )
{
	// Darcy flux
	real
		flow_thick1=minimum(sw1->Thickness(),sw1->LowerBoundary()-sw1->cell.get_saturated_depth()),
		flow_thick2=sw2 ? minimum(sw2->Thickness(),sw2->LowerBoundary()-sw2->cell.get_saturated_depth()) : flow_thick1;
	if (flow_thick1<=0 || flow_thick2<=0) return 0;
	real
		flow_thick=mean(flow_thick1,flow_thick2),
		// Topographic gradient
		Psi_t1=sw1->cell.z,
		Psi_t2=sw2 ? sw2->cell.z : m_right->Potential(),
		gradient=(Psi_t1-Psi_t2)/distance,
		// Transmissivity
		T1=sw1->Soil().Transmissivity(sw1->LowerBoundary()-flow_thick1,sw1->LowerBoundary(),sw1->Wetness()),
		T2=sw2 ? sw2->Soil().Transmissivity(sw2->LowerBoundary()-flow_thick2,sw2->LowerBoundary(),sw2->Wetness()) : T1,
		T = gradient>0 ? T1 : T2;
	return T*gradient*flow_width;

}

void cmf::upslope::connections::TopographicGradientDarcy::connect_cells( cmf::upslope::Cell & cell1,cmf::upslope::Cell & cell2,int start_at_layer/*=0*/ )
{
	real w=cell1.get_topology().flowwidth(cell2);
	if (w>0)
	{
		for (int i = start_at_layer; i < (start_at_layer>=0 ? cell1.layer_count() : 0) ; ++i)	{
			for (int j = start_at_layer; j < (start_at_layer>=0 ? cell2.layer_count() : 0) ; ++j)	{
				real ca=cell1.get_layer(i).GetFlowCrosssection(cell2.get_layer(j));
				if (ca>0)	{
					real d=cell1.get_layer(i).Location.distance3DTo(cell2.get_layer(i).Location);
				new TopographicGradientDarcy(cell1.get_layer(i),cell2.get_layer(j),w,d);
			}	}	}
	}

}

