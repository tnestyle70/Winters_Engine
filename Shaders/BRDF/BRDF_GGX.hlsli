#ifndef BRDF_GGX_HLSLI
#define BRDF_GGX_HLSLI

static const float PI = 3.14159265359f;

// Trowbridge-Reitz GGX NDF (Disney roughness convention: a = roughness^2)
float D_GGX(float NoH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float denom = NoH2 * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

// Schlick-GGX geometry term (Karis approximation for direct lighting)
float G_SchlickGGX(float NoX, float k)
{
    return NoX / (NoX * (1.0f - k) + k);
}

float G_Smith(float NoV, float NoL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return G_SchlickGGX(NoV, k) * G_SchlickGGX(NoL, k);
}

// Schlick Fresnel
float3 F_Schlick(float VoH, float3 F0)
{
    float Fc = pow(saturate(1.0f - VoH), 5.0f);
    return F0 + (1.0f - F0) * Fc;
}

// Full Cook-Torrance BRDF. Caller multiplies by light radiance separately.
float3 BRDF_CookTorrance(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float metallic,
    float roughness)
{
    float3 H = normalize(V + L);
    float NoV = saturate(dot(N, V)) + 1e-5f;
    float NoL = saturate(dot(N, L));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float D = D_GGX(NoH, roughness);
    float3 F = F_Schlick(VoH, F0);
    float G = G_Smith(NoV, NoL, roughness);

    float3 specular = (D * F * G) / (4.0f * NoV * NoL + 1e-5f);

    float3 kD = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NoL;
}

#endif // BRDF_GGX_HLSLI
