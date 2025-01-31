-- Vertex.NoClouds
#define NO_VELOCITY

#include "vbInputData.vert"
#include "lightingDefaults.vert"

layout(location = ATTRIB_FREE_START + 0) out vec4 vSunDirection; // vSunDirection.a = sunFade
layout(location = ATTRIB_FREE_START + 1) out vec4 vSunColour;  // vSunColour.a = vSunE
layout(location = ATTRIB_FREE_START + 2) out vec3 vAmbient;
layout(location = ATTRIB_FREE_START + 3) out vec3 vBetaR;
layout(location = ATTRIB_FREE_START + 4) out vec3 vBetaM;

void main(void){
    const NodeTransformData data = fetchInputData();
    VAR._vertexW = data._worldMatrix * dvd_Vertex;
    VAR._vertexW.xyz += dvd_CameraPosition;

    VAR._vertexWV = dvd_ViewMatrix * VAR._vertexW;
    computeLightVectors(data);
    setClipPlanes();
    gl_Position = dvd_ProjectionMatrix * VAR._vertexWV;
    gl_Position.z = gl_Position.w - Z_TEST_SIGMA;
}

--Vertex.Clouds

#define NO_VELOCITY
#define NEED_SCENE_DATA
#include "sceneData.cmn"
#include "vbInputData.vert"
#include "lightingDefaults.vert"

uniform vec3  dvd_nightSkyColour;
uniform vec3  dvd_moonColour;
uniform ivec2 dvd_useSkyboxes;
uniform vec3  dvd_RayleighCoeff;
uniform vec2  dvd_cloudLayerMinMaxHeight;
uniform uint  dvd_raySteps;
uniform float dvd_moonScale;
uniform float dvd_weatherScale;
uniform float dvd_sunIntensity;
uniform float dvd_sunPenetrationPower;
uniform float dvd_planetRadius;
uniform float dvd_cloudSphereRadius;
uniform float dvd_atmosphereOffset;
uniform float dvd_MieCoeff;
uniform float dvd_RayleighScale;
uniform float dvd_MieScaleHeight;
uniform uint  dvd_enableClouds;

layout(location = ATTRIB_FREE_START + 0) out vec4 vSunDirection; // vSunDirection.a = sunFade
layout(location = ATTRIB_FREE_START + 1) out vec4 vSunColour;  // vSunColour.a = vSunE
layout(location = ATTRIB_FREE_START + 2) out vec3 vAmbient;
layout(location = ATTRIB_FREE_START + 3) out vec3 vBetaR;
layout(location = ATTRIB_FREE_START + 4) out vec3 vBetaM;

// Mostly Preetham model stuff here
#define UP_DIR WORLD_Y_AXIS

// wavelength of used primaries, according to preetham
//const vec3 lambda = vec3(680E-9, 550E-9, 450E-9);
// this pre-calcuation replaces older TotalRayleigh(vec3 lambda) function:
// (8.0 * pow(pi, 3.0) * pow(pow(n, 2.0) - 1.0, 2.0) * (6.0 + 3.0 * pn)) / (3.0 * N * pow(lambda, vec3(4.0)) * (6.0 - 7.0 * pn))
const vec3 totalRayleigh = vec3(5.804542996261093E-6, 1.3562911419845635E-5, 3.0265902468824876E-5);

// mie stuff
// K coefficient for the primaries
const float v = 4.f;
const vec3  K = vec3(0.686f, 0.678f, 0.666f);
// MieConst = pi * pow( ( 2.0 * pi ) / lambda, vec3( v - 2.0 ) ) * K
const vec3 MieConst = vec3(1.8399918514433978E14, 2.7798023919660528E14, 4.0790479543861094E14);

/*
Atmospheric scattering based off of: https://www.shadertoy.com/view/XtBXDz
Author: valentingalea
*/
struct ray_t
{
    vec3 origin;
    vec3 direction;
};

bool intersectSphere(const in ray_t ray, inout float t0, inout float t1) {
    const vec3 rc = vec3(0.f) - ray.origin;
    const float atmosphere_radius = dvd_planetRadius + dvd_atmosphereOffset; // (m)
    const float radius2 = atmosphere_radius * atmosphere_radius;
    const float tca = dot(rc, ray.direction);
    const float d2 = dot(rc, rc) - tca * tca;
    if (d2 > radius2) {
        return false;
    }

    const float thc = sqrt(radius2 - d2);
    t0 = tca - thc;
    t1 = tca + thc;

    return true;
}

bool getSunLight(const in ray_t ray, inout float optical_depthR, inout float optical_depthM) {
    float t0 = 0.f;
    float t1 = 0.f;
    intersectSphere(ray, t0, t1);

    const float num_samples_light = dvd_raySteps * 0.5f;

    float march_pos = 0.f;
    const float march_step = t1 / float(num_samples_light);

    
    for (int i = 0; i < num_samples_light; ++i) {
        const vec3 s = ray.origin + ray.direction * (march_pos + 0.5f * march_step);
        const float height = length(s) - dvd_planetRadius;
        if (height < 0.f) {
            return false;
        }
        // scale height (m)
        // thickness of the atmosphere if its density were uniform
        optical_depthR += exp(-height / dvd_RayleighScale)  * march_step;
        optical_depthM += exp(-height / dvd_MieScaleHeight) * march_step;

        march_pos += march_step;
    }

    return true;
}

#define rayleigh_phase_func(MU) (3.f * (1.f + MU * MU) / (16.f * M_PI))
#define HG(costheta, g) (0.0795774715459f * (1.f - g * g) / (pow(1.f + g * g - 2.f * g * costheta, 1.5f)))

vec3 getIncidentLight(const in ray_t ray, const in vec3 sun_dir) {
    // "pierce" the atmosphere with the viewing ray
    float t0 = 0.f;
    float t1 = 0.f;
    if (!intersectSphere(ray, t0, t1)) {
        return vec3(0.f);
    }

    //0.76 more proper but 0.96 looks nice
#   define phaseMG 0.96f
    // scattering coefficients at sea level (m)
#   define betaR dvd_RayleighCoeff
#   define betaM vec3(dvd_MieCoeff)

    const float march_step = t1 / float(dvd_raySteps);

    // cosine of angle between view and light directions
    const float mu = dot(ray.direction, sun_dir);

    // Rayleigh and Mie phase functions
    const float phaseR = rayleigh_phase_func(mu);
    const float phaseM = HG(mu, phaseMG);

    // optical depth (or "average density")
    // represents the accumulated extinction coefficients
    // along the path, multiplied by the length of that path
    float optical_depthR = 0.f;
    float optical_depthM = 0.f;

    vec3 sumR = vec3(0.f);
    vec3 sumM = vec3(0.f);
    float march_pos = 0.f;

    for (int i = 0; i < dvd_raySteps; ++i) {
        const vec3 s = ray.origin + ray.direction * (march_pos + 0.5f * march_step);
        const float height = length(s) - dvd_planetRadius;

        // integrate the height scale
        const float hr = exp(-height / dvd_RayleighScale) * march_step;
        const float hm = exp(-height / dvd_MieScaleHeight) * march_step;

        optical_depthR += hr;
        optical_depthM += hm;

        // gather the sunlight
        const ray_t light_ray = ray_t(s, sun_dir);
        float optical_depth_lightR = 0.f;
        float optical_depth_lightM = 0.f;

        const bool overground = getSunLight(light_ray, optical_depth_lightR, optical_depth_lightM);
        if (overground) {
            const vec3 tau = betaR * 1.0f * (optical_depthR + optical_depth_lightR) +
                             betaM * 1.1f * (optical_depthM + optical_depth_lightM);
            const vec3 attenuation = exp(-tau);

            sumR += hr * attenuation;
            sumM += hm * attenuation;
        }

        march_pos += march_step;
    }

    return dvd_sunPenetrationPower * 
           (sumR * phaseR * betaR +
            sumM * phaseM * betaM);
}

float sunIntensity(float zenithAngleCos) {
    // constants for atmospheric scattering
    const float e = 2.71828182845904523536028747135266249775724709369995957;
    // earth shadow hack
    // cutoffAngle = pi / 1.95;
    const float cutoffAngle = 1.6110731556870734f;
    const float steepness = 1.5f;
    const float EE = 1000.f * dvd_sunIntensity;

    zenithAngleCos = clamp(zenithAngleCos, -1.f, 1.f);

    return EE * max(0.f, 1.f - pow(e, -((cutoffAngle - acos(zenithAngleCos)) / steepness)));
}

#define TotalMie(T) (0.434f * ((0.2f * T) * 10E-18) * MieConst)

void main() {

    const float rayleigh = 1.f;
    const float turbidity = 2.f;
    const float mieCoefficient = 0.005f;

    const NodeTransformData data = fetchInputData();
    VAR._vertexW = data._worldMatrix * dvd_Vertex;
    VAR._vertexW.xyz += dvd_CameraPosition;

    VAR._vertexWV = dvd_ViewMatrix * VAR._vertexW;
    computeLightVectors(data);
    setClipPlanes();
    gl_Position = dvd_ProjectionMatrix * VAR._vertexWV;
    gl_Position.z = gl_Position.w - Z_TEST_SIGMA;

    vSunDirection.xyz = normalize(dvd_sunPosition.xyz);

    const float vSunE = sunIntensity(dot(vSunDirection.xyz, UP_DIR));

    const float vSunFade = 1.f - Saturate(1.f - exp((dvd_sunPosition.y / 450000.0f)));

    const float rayleighCoefficient = rayleigh - (1.f * (1.f - vSunFade));

    // extinction (absorbtion + out scattering)
    // rayleigh coefficients
    vBetaR = totalRayleigh * rayleighCoefficient;

    // mie coefficients
    vBetaM = TotalMie(turbidity) * mieCoefficient;

    ray_t ray = ray_t(vec3(0.f, dvd_planetRadius + 1.f, 0.f), normalize(vSunDirection.xyz + vec3(0.01f, 0.01f, 0.f)));
    vSunColour.rgb = getIncidentLight(ray, vSunDirection.xyz);

    ray = ray_t(vec3(0.f, dvd_planetRadius + 1.f, 0.f), normalize(vec3(0.4f, 0.1f, 0.f)));
    vAmbient = getIncidentLight(ray, vSunDirection.xyz);
    vSunDirection.a = vSunFade;
    vSunColour.a = vSunE;
}

--Fragment.Clouds

#if !defined(PRE_PASS)
layout(early_fragment_tests) in;
#endif //!PRE_PASS

#define NO_VELOCITY

layout(location = ATTRIB_FREE_START + 0) in vec4 vSunDirection; //vSunDirection.a = sun fade
layout(location = ATTRIB_FREE_START + 1) in vec4 vSunColour; // vSunColour.a = vSunE
layout(location = ATTRIB_FREE_START + 2) in vec3 vAmbient;
layout(location = ATTRIB_FREE_START + 3) in vec3 vBetaR;
layout(location = ATTRIB_FREE_START + 4) in vec3 vBetaM;

DESCRIPTOR_SET_RESOURCE(PER_DRAW_SET, TEXTURE_UNIT0)     uniform samplerCubeArray texSky;
DESCRIPTOR_SET_RESOURCE(PER_DRAW_SET, TEXTURE_HEIGHTMAP) uniform sampler2DArray weather;
DESCRIPTOR_SET_RESOURCE(PER_DRAW_SET, TEXTURE_UNIT1)     uniform sampler2DArray curl;
DESCRIPTOR_SET_RESOURCE(PER_DRAW_SET, TEXTURE_SPECULAR)  uniform sampler3D worl;
DESCRIPTOR_SET_RESOURCE(PER_DRAW_SET, TEXTURE_NORMALMAP) uniform sampler3D perlworl;

uniform vec3 dvd_nightSkyColour;
uniform vec3 dvd_moonColour;
uniform ivec2 dvd_useSkyboxes;
uniform vec3 dvd_RayleighCoeff;
uniform vec2 dvd_cloudLayerMinMaxHeight;
uniform uint  dvd_raySteps;
uniform float dvd_moonScale;
uniform float dvd_weatherScale;
uniform float dvd_sunIntensity;
uniform float dvd_sunPenetrationPower;
uniform float dvd_planetRadius;
uniform float dvd_cloudSphereRadius;
uniform float dvd_atmosphereOffset;
uniform float dvd_MieCoeff;
uniform float dvd_RayleighScale;
uniform float dvd_MieScaleHeight;
uniform uint  dvd_enableClouds;

#define dvd_useDaySkybox (dvd_useSkyboxes.x == 1)
#define dvd_useNightSkybox (dvd_useSkyboxes.y == 1)

#define NO_POST_FX

#define NEED_SCENE_DATA
#include "sceneData.cmn"
#include "utility.frag"
#include "output.frag"

#define UP_DIR WORLD_Y_AXIS

float sky_b_radius = 0.f;
float sky_t_radius = 0.f;

//precomputed 1/U2Tone(40)
#define cwhiteScale 1.1575370919881305f

// 3.0 / ( 16.0 * pi )
#define THREE_OVER_SIXTEENPI 0.05968310365946075f
// 1.0 / ( 4.0 * pi )
#define ONE_OVER_FOURPI 0.07957747154594767

vec3 U2Tone(const vec3 x) {
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// the maximal dimness of a dot ( 0.0->1.0   0.0 = all dots bright,  1.0 = maximum variation )
float SimplexPolkaDot3D(in vec3 P, in float density)
{
    // simplex math constants
    const vec3 SKEWFACTOR = vec3(1.f / 3.f);
    const vec3 UNSKEWFACTOR = vec3(1.f / 6.f);

    const vec3 SIMPLEX_CORNER_POS = vec3(0.5f);

    // calculate the simplex vector and index math (sqrt( 0.5 ) height of simplex pyramid.)
    const vec3 Pn = P * 0.70710678118654752440084436210485; // scale space so we can have an approx feature size of 1.0  ( optional )

    // Find the vectors to the corners of our simplex pyramid
    const vec3 Pi = floor(Pn + vec3(dot(Pn, SKEWFACTOR)));
    const vec3 x0 = Pn - Pi + vec3(dot(Pi, UNSKEWFACTOR));
    const vec3 g = step(x0.yzx, x0.xyz);
    const vec3 l = vec3(1.f) - g;
    const vec3 Pi_1 = min(g.xyz, l.zxy);
    const vec3 Pi_2 = max(g.xyz, l.zxy);
    const vec3 x1 = x0 - Pi_1 + UNSKEWFACTOR;
    const vec3 x2 = x0 - Pi_2 + SKEWFACTOR;
    const vec3 x3 = x0 - SIMPLEX_CORNER_POS;

    // pack them into a parallel-friendly arrangement
    vec4 v1234_x = vec4(x0.x, x1.x, x2.x, x3.x);
    vec4 v1234_y = vec4(x0.y, x1.y, x2.y, x3.y);
    vec4 v1234_z = vec4(x0.z, x1.z, x2.z, x3.z);

    const vec3 v1_mask = Pi_1;
    const vec3 v2_mask = Pi_2;

    const vec2 OFFSET = vec2(50.f, 161.f);
    const float DOMAIN = 69.f;
    const float SOMELARGEFLOAT = 6351.29681f;
    const float ZINC = 487.500388f;

    // truncate the domain
    const vec3 gridcell = Pi - floor(Pi * (1.f / DOMAIN)) * DOMAIN;
    const vec3 gridcell_inc1 = step(gridcell, vec3(DOMAIN - 1.5)) * (gridcell + vec3(1.f));

    // compute x*x*y*y for the 4 corners
    vec4 Pp = vec4(gridcell.xy, gridcell_inc1.xy) + vec4(OFFSET, OFFSET);
    Pp *= Pp;

    const vec4 V1xy_V2xy = mix(vec4(Pp.xy, Pp.xy), vec4(Pp.zw, Pp.zw), vec4(v1_mask.xy, v2_mask.xy)); // apply mask for v1 and v2
    Pp = vec4(Pp.x, V1xy_V2xy.x, V1xy_V2xy.z, Pp.z) * vec4(Pp.y, V1xy_V2xy.y, V1xy_V2xy.w, Pp.w);

    vec2 V1z_V2z = vec2(gridcell_inc1.z);
    if (v1_mask.z < 0.5f) {
        V1z_V2z.x = gridcell.z;
    }
    if (v2_mask.z < 0.5f) {
        V1z_V2z.y = gridcell.z;
    }

    const vec4 temp = vec4(SOMELARGEFLOAT) + vec4(gridcell.z, V1z_V2z.x, V1z_V2z.y, gridcell_inc1.z) * ZINC;
    const vec4 mod_vals = vec4(1.f) / (temp);
    // compute the final hash
    const vec4 hash = fract(Pp * mod_vals);

    // apply user controls

    // scale to a 0.0->1.0 range.  2.0 / sqrt( 0.75 )
#   define INV_SIMPLEX_TRI_HALF_EDGELEN 2.3094010767585030580365951220078f
    const float radius = INV_SIMPLEX_TRI_HALF_EDGELEN;///(1.15-density);

    v1234_x *= radius;
    v1234_y *= radius;
    v1234_z *= radius;
    // return a smooth falloff from the closest point.  ( we use a f(x)=(1.0-x*x)^3 falloff )
    const vec4 point_distance = max(vec4(0.f), vec4(1.f) - (v1234_x * v1234_x + v1234_y * v1234_y + v1234_z * v1234_z));

    const vec4 b = pow(min(vec4(1.f), max(vec4(0.f), (vec4(density) - hash) * (1.f / density))), vec4(1.f / density));
    return dot(b, point_distance * point_distance * point_distance);
}

#define HG(costheta, g) (0.0795774715459f * (1.f - g * g) / (pow(1.f + g * g - 2.f * g * costheta, 1.5f)))

//This implementation of the preetham model is a modified: https://github.com/mrdoob/three.js/blob/master/examples/js/objects/Sky.js
//written by: zz85 / https://github.com/zz85
vec3 preetham(in vec3 rayDirection) {
    const float vSunFade = vSunDirection.a;
    const float vSunE = vSunColour.a;

    const float W = 1000.f;
    const float mieDirectionalG = 0.8f;
    // optical length at zenith for molecules
    const float rayleighZenithLength = 8.4E3;
    const float mieZenithLength = 1.25E3;
    const float whiteScale = 1.0748724675633854f; // 1.0 / U2Tone(1000.0)

    // 66 arc seconds -> degrees, and the cosine of that
    const float sunAngularDiameterCos = 0.999956676946448443553574619906976478926848692873900859324;

    // optical length
    // cutoff angle at 90 to avoid singularity in next formula.
    const float zenithAngle = acos(max(0.f, dot(UP_DIR, rayDirection)));

    const float inv = 1.f / (cos(zenithAngle) + 0.15f * pow(93.885f - ((zenithAngle * 180.f) / M_PI), -1.253f));
    const float sR = rayleighZenithLength * inv;
    const float sM = mieZenithLength * inv;

    // combined extinction factor
    const vec3 Fex = exp(-(vBetaR * sR + vBetaM * sM));

    // in scattering
    const float cosTheta = dot(rayDirection, vSunDirection.xyz);
    const float rPhase = THREE_OVER_SIXTEENPI * (1.f + pow(cosTheta * 0.5f + 0.5f, 2.f));
    const vec3 betaRTheta = vBetaR * rPhase;

    const float mPhase = HG(cosTheta, mieDirectionalG);
    const vec3 betaMTheta = vBetaM * mPhase;

    vec3 Lin = pow(vSunE * ((betaRTheta + betaMTheta) / (vBetaR + vBetaM)) * (1.f - Fex), vec3(1.5f));
    Lin *= mix(vec3(1.f), pow(vSunE * ((betaRTheta + betaMTheta) / (vBetaR + vBetaM)) * Fex, vec3(0.5f)), Saturate(pow(1.f - dot(UP_DIR, vSunDirection.xyz), 5.f)));

    vec3 L0 = vec3(0.5f) * Fex;

    // composition + solar disc
    const float sundisk = smoothstep(sunAngularDiameterCos, sunAngularDiameterCos + 0.00002f, cosTheta);
    L0 += (vSunE * 19000.f * Fex) * sundisk;

    const vec3 texColor = (Lin + L0) * 0.04f + vec3(0.4f, 0.0003f, 0.00075f);
    const vec3 curr = U2Tone(texColor);
    const vec3 color = curr * whiteScale;
    const vec3 retColor = pow(color, vec3(1.f / (1.2f + (1.2f * vSunFade))));
    return retColor;
}

const vec3 RANDOM_VECTORS[6] = vec3[6]
(
    vec3( 0.38051305f,  0.92453449f, -0.02111345f),
    vec3(-0.50625799f, -0.03590792f, -0.86163418f),
    vec3(-0.32509218f, -0.94557439f,  0.01428793f),
    vec3( 0.09026238f, -0.27376545f,  0.95755165f),
    vec3( 0.28128598f,  0.42443639f, -0.86065785f),
    vec3(-0.16852403f,  0.14748697f,  0.97460106f)
);

// fractional value for sample position in the cloud layer
float GetHeightFractionForPoint(in float inPosition) {
    // get global fractional position in cloud zone
    return Saturate((inPosition - sky_b_radius) / (sky_t_radius - sky_b_radius));
}

vec4 mixGradients(in float cloudType) {
    const vec4 STRATUS_GRADIENT = vec4(0.02f, 0.05f, 0.09f, 0.11f);
    const vec4 STRATOCUMULUS_GRADIENT = vec4(0.02f, 0.2f, 0.48f, 0.625f);
    const vec4 CUMULUS_GRADIENT = vec4(0.01f, 0.0625f, 0.78f, 1.0f); // these fractions would need to be altered if cumulonimbus are added to the same pass
    const float stratus = 1.0f - Saturate(cloudType * 2.0f);
    const float stratocumulus = 1.0f - abs(cloudType - 0.5f) * 2.0f;
    const float cumulus = Saturate(cloudType - 0.5f) * 2.0f;

    return STRATUS_GRADIENT * stratus + STRATOCUMULUS_GRADIENT * stratocumulus + CUMULUS_GRADIENT * cumulus;
}

float densityHeightGradient(in float heightFrac, in float cloudType) {
    const vec4 cloudGradient = mixGradients(cloudType);
    return smoothstep(cloudGradient.x, cloudGradient.y, heightFrac) - smoothstep(cloudGradient.z, cloudGradient.w, heightFrac);
}

float intersectSphere(in vec3 pos, in vec3 dir, in float r) {
    const float a = dot(dir, dir);
    const float b = 2.f * dot(dir, pos);
    const float c = dot(pos, pos) - (r * r);
    const float d = sqrt((b * b) - 4.f * a * c);
    const float p = -b - d;
    const float p2 = -b + d;

    return max(p, p2) / (2.f * a);
}

float density(vec3 p, in vec3 weather, in bool hq, in float LOD) {
    const float time = MSToSeconds(dvd_TimeMS);

    p.x += time * 20.f;
    //p.z -= time * 5.f;

    const float height_fraction = GetHeightFractionForPoint(length(p));
    const vec4 n = textureLod(perlworl, p * 0.0003f, LOD);

    const float fbm = n.g * 0.625f + n.b * 0.25f + n.a * 0.125f;
    const float g = densityHeightGradient(height_fraction, 0.5f);

    float base_cloud = ReMap(n.r, -(1.f - fbm), 1.f, 0.f, 1.f);
    const float cloud_coverage = smoothstep(0.6f, 1.3f, weather.x);

    base_cloud = ReMap(base_cloud * g, 1.f - cloud_coverage, 1.f, 0.f, 1.f);
    base_cloud *= cloud_coverage;
    if (hq) {
        const vec2 whisp = texture(curl, vec3(p.xy * 0.0003f, 0)).xy;

        p.xy += whisp * 400.f * (1.f - height_fraction);
        const vec3 hn = texture(worl, p * 0.004f, LOD - 2.f).xyz;
        float hfbm = hn.r * 0.625f + hn.g * 0.25f + hn.b * 0.125f;
        hfbm = mix(hfbm, 1.f - hfbm, Saturate(height_fraction * 3.f));
        base_cloud = ReMap(base_cloud, hfbm * 0.2f, 1.f, 0.f, 1.f);
    }
    return Saturate(base_cloud);
}

vec4 march(in vec3 colourIn, in vec3 ambientIn, in vec3 pos, in vec3 end, in vec3 dir, in int depth) {
    float T = 1.f;
    float alpha = 0.f;
    vec3 p = pos;

    const float ss = length(dir);

    const float t_dist = sky_t_radius - sky_b_radius;
    const float lss = t_dist / float(depth);
    const vec3 ldir = vSunDirection.xyz * ss;

    vec3 L = vec3(0.f);

    const float costheta = dot(normalize(ldir), normalize(dir));
    const float phase = max(max(HG(costheta, 0.6f), HG(costheta, (0.99f - 1.3f * normalize(ldir).y))), HG(costheta, -0.3f));

    for (int i = 0; i < depth; ++i) {
        p += dir;
        const float height_fraction = GetHeightFractionForPoint(length(p));

        const vec3 weather_sample = texture(weather, vec3(p.xz * dvd_weatherScale, 0)).xyz;

        const float t = density(p, weather_sample, true, 0.f);
        const float dt = exp(-0.5f * t * ss);

        T *= dt;
        vec3 lp = p;
        const float ld = 0.5f;

        float ncd = 0.f;
        float cd = 0.f;
        if (t > 0.f) { //calculate lighting, but only when we are in a non-zero density point
            for (int j = 0; j < 6; ++j) {
                lp += (ldir + (RANDOM_VECTORS[j] * float(j + 1)) * lss);

                const vec3 lweather = texture(weather, vec3(lp.xz * dvd_weatherScale, 0)).xyz;
                const float lt = density(lp, lweather, false, float(j));

                cd += lt;
                ncd += (lt * (1.f - (cd * (1.f / (lss * 6.f)))));
            }
            lp += ldir * 12.f;

            const vec3 lweather = texture(weather, vec3(lp.xz * dvd_weatherScale, 0)).xyz;
            const float lt = density(lp, lweather, false, 5.f);

            cd += lt;
            ncd += (lt * (1.f - (cd * (1.f / (lss * 18.f)))));

            const float beers = max(exp(-ld * ncd * lss), exp(-ld * 0.25f * ncd * lss) * 0.7f);
            const float powshug = 1.f - exp(-ld * ncd * lss * 2.f);

            const vec3 ambient = 5.f * ambientIn * mix(0.15f, 1.f, height_fraction);
            const vec3 sunC = pow(colourIn, vec3(0.75f));

            L += (ambient + sunC * beers * powshug * 2.0 * phase) * (t)*T * ss;
            alpha += (1.f - dt) * (1.f - alpha);
        }
    }

    return vec4(L, alpha);
}

vec3 nightColour(in vec3 rayDirection, in float lerpValue) {

    vec3 skyColour = dvd_nightSkyColour;
    if (dvd_useNightSkybox && lerpValue > 0.25f) {
        const vec3 sky = texture(texSky, vec4(rayDirection, 1.f)).rgb;
        skyColour = (skyColour + sky) - (skyColour * sky);
    }

    const float star = SimplexPolkaDot3D(rayDirection * 100.f, 0.15f) + SimplexPolkaDot3D(rayDirection * 150.f, 0.25f) * 0.7f;

    const vec3 ret = skyColour + 
                     max(0.f, (star - smoothstep(0.2f, 0.95f, 0.5f - 0.5f * rayDirection.y))) +
                     skyColour * (1.f - smoothstep(-0.1f, 0.45f, rayDirection.y));


    //Need to add a little bit of atmospheric effects, both for when the moon is high and for the end when color comes into the sky
    //For moon halo
    const vec3 moonpos = -vSunDirection.xyz;
    const vec3 moonposNorm = normalize(moonpos);
    const float d = length(rayDirection - moonposNorm);

    const vec3 moonColour = vec3(smoothstep(1.0f - (dvd_moonScale), 1.f, dot(rayDirection, moonposNorm))) +
                            0.4f * exp(-4.f * d) * dvd_moonColour +
                            0.2f * exp(-2.f * d);

    return U2Tone(ret + moonColour);
}

vec3 dayColour(in vec3 rayDirection, in float lerpValue) {
    const vec3 colour = preetham(rayDirection);

    if (dvd_useDaySkybox && lerpValue < 0.2f) {
        const vec3 sky = texture(texSky, vec4(rayDirection, 0.f)).rgb;

        return mix((colour + sky) - (colour * sky),
                   colour,
                   Luminance(colour.rgb));
    }

    return colour;
}

//ref: https://github.com/clayjohn/realtime_clouds
vec3 computeClouds(in vec3 rayDirection, in vec3 skyColour, in float lerpValue) {
    if (rayDirection.y > 0.0) {
        const vec3 cloudColour = mix(vSunColour.rgb, dvd_moonColour * 0.05f, lerpValue);
        const vec3 cloudAmbient = mix(vAmbient, vec3(0.05f), lerpValue);

        const vec3 camPos = vec3(0.f, dvd_cloudSphereRadius, 0.f);
        const vec3 start = camPos + rayDirection * intersectSphere(camPos, rayDirection, sky_b_radius);
        const vec3 end = camPos + rayDirection * intersectSphere(camPos, rayDirection, sky_t_radius);

        const float t_dist = sky_t_radius - sky_b_radius;

        const float shelldist = (length(end - start));
        const float steps = (mix(96.f, 54.f, dot(rayDirection, UP_DIR)));
        const float dmod = smoothstep(0.f, 1.f, (shelldist / t_dist) / 14.f);
        const float s_dist = mix(t_dist, t_dist * 4.f, dmod) / (steps);

        const vec3 raystep = rayDirection * s_dist;

        vec4 volume = march(cloudColour, cloudAmbient, start, end, raystep, int(steps));
        volume.xyz = sqrt(abs(U2Tone(volume.xyz) * cwhiteScale));

        const vec3 background = volume.a < 0.99f ? skyColour : vec3(0.f);
        return vec3(background * (1.f - volume.a) + volume.xyz * volume.a);
    }

    return skyColour;
}

vec3 getSkyColour(in vec3 rayDirection, in float lerpValue) {
    return mix(dayColour(rayDirection, lerpValue), nightColour(rayDirection, lerpValue), lerpValue);
}

vec3 getRawAlbedo(in vec3 rayDirection, in float lerpValue) {
    return lerpValue <= 0.5f ? (dvd_useDaySkybox ? texture(texSky, vec4(rayDirection, 0.f)).rgb : vec3(0.4f))
                             : (dvd_useNightSkybox ? texture(texSky, vec4(rayDirection, 1.f)).rgb : vec3(0.2f));
}

vec3 atmosphereColour(in vec3 rayDirection, in float lerpValue) {
    const vec3 skyColour = getSkyColour(rayDirection, lerpValue);
    return dvd_enableClouds != 0u ? computeClouds(rayDirection, skyColour, lerpValue) : skyColour;
}

void main() {
    sky_b_radius = dvd_cloudSphereRadius + dvd_cloudLayerMinMaxHeight.x;//bottom of cloud layer
    sky_t_radius = dvd_cloudSphereRadius + dvd_cloudLayerMinMaxHeight.y;//top of cloud layer

    // Guess work based on what "look right"
    const float lerpValue = Saturate(2.95f * (GetSunDirection().y + 0.15f));
    const vec3 rayDirection = normalize(VAR._vertexW.xyz - dvd_CameraPosition);
#if defined(MAIN_DISPLAY_PASS)
    vec3 ret = vec3(0.f);
    switch (dvd_MaterialDebugFlag) {
        case DEBUG_ALBEDO:        ret = getRawAlbedo(rayDirection, lerpValue); break;
        case DEBUG_LIGHTING:      ret = getSkyColour(rayDirection, lerpValue); break;
        case DEBUG_SPECULAR:      
        case DEBUG_SSAO:
        case DEBUG_IBL:
        case DEBUG_KS:            ret = vec3(0.f); break;
        case DEBUG_UV:            ret = vec3(fract(rayDirection)); break;
        case DEBUG_EMISSIVE:      ret = getSkyColour(rayDirection, lerpValue); break;
        case DEBUG_ROUGHNESS:     ret = vec3(1.f); break;
        case DEBUG_METALNESS:     ret = vec3(0.f); break;
        case DEBUG_NORMALS:       ret = normalize(mat3(dvd_InverseViewMatrix) * VAR._normalWV); break;
        case DEBUG_TANGENTS:
        case DEBUG_BITANGENTS:    ret = vec3(0.0f); break;
        case DEBUG_SHADOW_MAPS:
        case DEBUG_CSM_SPLITS:    ret = vec3(1.0f); break;
        case DEBUG_LIGHT_HEATMAP:
        case DEBUG_DEPTH_CLUSTERS:
        case DEBUG_DEPTH_CLUSTER_AABBS:
        case DEBUG_REFRACTIONS:
        case DEBUG_REFLECTIONS:
        case DEBUG_MATERIAL_IDS:
        case DEBUG_SHADING_MODE:  ret = vec3(0.0f); break;
        default:                  ret = atmosphereColour(rayDirection, lerpValue); break;
    }

#else //MAIN_DISPLAY_PASS
    const vec3 ret = atmosphereColour(rayDirection, lerpValue);
#endif //MAIN_DISPLAY_PASS
    writeScreenColour(vec4(ret, 1.f));
}

-- Fragment.PrePass

#define NO_VELOCITY
#include "prePass.frag"

void main() {

    writeGBuffer(VAR._normalWV, 1.f);
}