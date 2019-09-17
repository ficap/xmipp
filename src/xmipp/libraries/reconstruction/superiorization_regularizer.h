/***************************************************************************
 *
 * Authors:     Edgar Garduno Angeles (edgargar@ieee.org)
 * Created on: Sep 6, 2019
 *
 * Department of Computer Science, Institute for Applied Mathematics
 * and Systems Research (IIMAS), National Autonomous University of
 * Mexico (UNAM)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 *
 *  All comments concerning this program package may be sent to the
 *  e-mail address 'xmipp@cnb.csic.es'
 ***************************************************************************/
#ifndef SUPERIORIZATION_REGULARIZER_HH
#define SUPERIORIZATION_REGULARIZER_HH

#include <functional>
#include <core/xmipp_program.h>

#include "total_variation_namespace.h"

// template <typename R, typename ...ARGS> using function = R(*)(ARGS...);
// template< class R, class... Args > class function<R(Args...)>;
template<class T>
class SuperRegular: public MultidimArray<T>
{
 public:
	enum classType{ITV,WTV};

 private:
 //
 // Methods
 //

 public:
	SuperRegular();
	SuperRegular(String &StrType);
	void set(std::string StrType);
	double operator ()(const MultidimArray<T>& x);
	void nav(const MultidimArray<T>& x,MultidimArray<T>& v);
 protected:

 private:
    classType RegType;
	std::function<double(const MultidimArray<T>& x)> phi;
	std::function<void(const MultidimArray<T>& x,MultidimArray<T>& v)> navPhi;
};

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/****************** Definition of Class Methods ********************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

/**
**
** Basic constructor.
**
*/
template<class T>
SuperRegular<T>::SuperRegular()
{
 //phi = std::bind(&itv::tv,this,std::placeholders::_1);
 //nav = std::bind(&itv::vtv,this,std::placeholders::_1,std::placeholders::_2);
 phi = itv::tv;
 navPhi = itv::vtv;
 RegType = SuperRegular::ITV;
}

/**
**
** Constructor from a name (string).
**
*/
template<class T>
SuperRegular<T>::SuperRegular(String &StrType)
{
 if(StrType == std::string("ITV")){
	phi = itv::tv;
	navPhi = itv::vtv;
    RegType = SuperRegular::ITV;
   }
}

/**
**
** Method to set the desired function to be used as a second criterion.
**
*/
template<class T>
void SuperRegular<T>::set(std::string StrType)
{
 if(StrType == std::string("ITV")){
    phi = itv::tv;
    navPhi = itv::vtv;
    RegType = SuperRegular::ITV;
   }
}

/**
**
** Method to set the desired function to be used as a second criterion.
**
*/
template<class T>
double SuperRegular<T>::operator ()(const MultidimArray<T>& x)
{
 return phi(x);
}

/**
**
** Method to set the desired function to be used as a second criterion.
**
*/
template<class T>
void SuperRegular<T>::nav(const MultidimArray<T>& x, MultidimArray<T>& v)
{
 navPhi(x,v);
}

#endif /* SUPERIORIZATION_REGULARIZER_HH */
