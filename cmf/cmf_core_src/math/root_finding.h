// This file is part of cmf, but copied and modified from codereview.stackexchange, 
// original author: Grant Williams
// Source: https://codereview.stackexchange.com/q/103762
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

// Orignial Author's notice
/*******************************************************************************
*
* Grant Williams
*
* Version 1.0.0
* August 27, 2015
*
* Test Program for Brent's Method Function.
*
* Brent's method makes use of the bisection method, the secant method, and inverse quadratic interpolation in one algorithm.
*
* To Compile Please use icc -std=c++11 if using intel or g++ -std=c++11 if using GCC.
*
********************************************************************************/

namespace cmf {
	namespace math {
		template <typename Func>
		double brents_method(Func f, double lower_bound, double upper_bound, double offset = 0, double tolerance = 1e-12, double max_iterations = 100);
	}
}



