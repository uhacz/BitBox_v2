#pragma once

#include "vmath.h"

mat44_t inverse( const mat44_t& m )
{
    const float mA = m.c0.x;
    const float mB = m.c0.y;
    const float mC = m.c0.z;
    const float mD = m.c0.w;
    const float mE = m.c1.x;
    const float mF = m.c1.y;
    const float mG = m.c1.z;
    const float mH = m.c1.w;
    const float mI = m.c2.x;
    const float mJ = m.c2.y;
    const float mK = m.c2.z;
    const float mL = m.c2.w;
    const float mM = m.c3.x;
    const float mN = m.c3.y;
    const float mO = m.c3.z;
    const float mP = m.c3.w;

    float tmp0 = ( ( mK * mD ) - ( mC * mL ) );
    float tmp1 = ( ( mO * mH ) - ( mG * mP ) );
    float tmp2 = ( ( mB * mK ) - ( mJ * mC ) );
    float tmp3 = ( ( mF * mO ) - ( mN * mG ) );
    float tmp4 = ( ( mJ * mD ) - ( mB * mL ) );
    float tmp5 = ( ( mN * mH ) - ( mF * mP ) );

    vec4_t res0, res1, res2, res3;

    res0.x = ( ( ( mJ * tmp1 ) - ( mL * tmp3 ) ) - ( mK * tmp5 ) );
    res0.y = ( ( ( mN * tmp0 ) - ( mP * tmp2 ) ) - ( mO * tmp4 ) );
    res0.z = ( ( ( mD * tmp3 ) + ( mC * tmp5 ) ) - ( mB * tmp1 ) );
    res0.w = ( ( ( mH * tmp2 ) + ( mG * tmp4 ) ) - ( mF * tmp0 ) );

    const float detInv = ( 1.0f / ( ( ( ( mA * res0.x ) + ( mE * res0.y ) ) + ( mI * res0.z ) ) + ( mM * res0.w ) ) );

    res1.x = ( mI * tmp1 );
    res1.y = ( mM * tmp0 );
    res1.z = ( mA * tmp1 );
    res1.w = ( mE * tmp0 );
    res3.x = ( mI * tmp3 );
    res3.y = ( mM * tmp2 );
    res3.z = ( mA * tmp3 );
    res3.w = ( mE * tmp2 );
    res2.x = ( mI * tmp5 );
    res2.y = ( mM * tmp4 );
    res2.z = ( mA * tmp5 );
    res2.w = ( mE * tmp4 );
    tmp0 = ( ( mI * mB ) - ( mA * mJ ) );
    tmp1 = ( ( mM * mF ) - ( mE * mN ) );
    tmp2 = ( ( mI * mD ) - ( mA * mL ) );
    tmp3 = ( ( mM * mH ) - ( mE * mP ) );
    tmp4 = ( ( mI * mC ) - ( mA * mK ) );
    tmp5 = ( ( mM * mG ) - ( mE * mO ) );
    res2.x = ( ( ( mL * tmp1 ) - ( mJ * tmp3 ) ) + res2.x );
    res2.y = ( ( ( mP * tmp0 ) - ( mN * tmp2 ) ) + res2.y );
    res2.z = ( ( ( mB * tmp3 ) - ( mD * tmp1 ) ) - res2.z );
    res2.w = ( ( ( mF * tmp2 ) - ( mH * tmp0 ) ) - res2.w );
    res3.x = ( ( ( mJ * tmp5 ) - ( mK * tmp1 ) ) + res3.x );
    res3.y = ( ( ( mN * tmp4 ) - ( mO * tmp0 ) ) + res3.y );
    res3.z = ( ( ( mC * tmp1 ) - ( mB * tmp5 ) ) - res3.z );
    res3.w = ( ( ( mG * tmp0 ) - ( mF * tmp4 ) ) - res3.w );
    res1.x = ( ( ( mK * tmp3 ) - ( mL * tmp5 ) ) - res1.x );
    res1.y = ( ( ( mO * tmp2 ) - ( mP * tmp4 ) ) - res1.y );
    res1.z = ( ( ( mD * tmp5 ) - ( mC * tmp3 ) ) + res1.z );
    res1.w = ( ( ( mH * tmp4 ) - ( mG * tmp2 ) ) + res1.w );
    return mat44_t(
        ( res0 * detInv ),
        ( res1 * detInv ),
        ( res2 * detInv ),
        ( res3 * detInv )
    );
}
