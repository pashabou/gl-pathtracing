#version 430 core

const int MULTISAMPLING_RAYS_EMITTED = 4;
const int NUM_BOUNCES = 10;
const bool ANTIALIASING = true;
const bool DEPTH_OF_FIELD = true;
const bool MOTION_BLUR = true;

const float PI = 3.14159265359;
const float MAX_SCENE_BOUNDS = 100.0;

const int LAMBERTIAN = 0x00000001;
const int METAL = 0x00000002;
const int DIELECTRIC = 0x00000004;
const int EMISSIVE = 0x00000008;

layout (local_size_x = 16, local_size_y = 8) in;
layout (rgba32f, binding = 0) uniform image2D framebuffer;
layout (binding = 1) uniform atomic_uint raysTraced;

uniform float globalTime;
uniform int frameCount;
uniform vec3 eye;
uniform vec3 ray00;
uniform vec3 ray01;
uniform vec3 ray10;
uniform vec3 ray11;
uniform mat3 transposeInverseViewMatrix;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
	vec2 ip = floor(p);
	vec2 u = fract(p);
	u = u * u * (3.0 - 2.0 * u);
	
	float res = mix(
		mix(rand(ip), rand(ip + vec2(1.0, 0.0)), u.x),
		mix(rand(ip + vec2(0.0, 1.0)), rand(ip + vec2(1.0, 1.0)), u.x), u.y);
	return res * res;
}

vec3 sphereRand(vec2 co) {
    vec3 x = vec3(rand(co + 10), rand(co + 20), rand(co + 30));
    do {
        x = vec3(rand(x.xy), rand(x.yz), rand(x.xz)) * 2.0 - 1.0;
    } while (length(x) * length(x) >= 1);
    return x;
}

float schlick(float cosine, float refractionIndex) {
    float r = (1.0 - refractionIndex) / (1.0 + refractionIndex);
    return mix(pow(1.0 - cosine, 5.0), 1.0, r * r);
}

struct box {
    vec3 min;
    vec3 max;
    vec3 color;

    int material;
    float fuzz;
    float refractionIndex;
};

struct sphere {
    vec3 center;
    float radius;
    vec3 color;

    int material;
    float fuzz;
    float refractionIndex;
};

struct hitinfo {
    float intersection;
    vec3 point;
    vec3 color;
    vec3 normal;
    
    int material;
    float fuzz;
    float refractionIndex;
};

const box boxes[] = {
	// FLOOR
    {
        vec3(-5.0, -0.1, -5.0),
        vec3(5.0, 0.0, 5.0),
        vec3(0.9),
		LAMBERTIAN,
        float(0.4),
        float(0.0)
    },
	// AXIS INDICATORS
    {
        vec3(0.0, 0.0, 0.0),
        vec3(0.2, 0.2, 0.2),
        vec3(1.0),
		LAMBERTIAN,
        float(0.0),
        float(0.0)
    },
    {
        vec3(0.0, 0.2, 0.0),
        vec3(0.2, 1.0, 0.2),
        vec3(0.0, 1.0, 0.0),
		LAMBERTIAN,
        float(0.0),
        float(0.0)
    },
    {
        vec3(0.2, 0.0, 0.0),
        vec3(1.0, 0.2, 0.2),
        vec3(1.0, 0.0, 0.0),
		LAMBERTIAN,
        float(0.0),
        float(0.0)
    },
    {
        vec3(0.0, 0.0, 0.2),
        vec3(0.2, 0.2, 1.0),
        vec3(0.0, 0.0, 1.0),
		LAMBERTIAN,
        float(0.0),
        float(0.0)
    },
	// LIGHTS
	{
       vec3(-5.0, 6.99, -6.0),
       vec3(5.0, 7.0, -4.0),
       vec3(1.4),
       LAMBERTIAN | EMISSIVE,
       float(0.4),
       float(0.0)
   },
	{
		vec3(-5.0, 6.99, -2.0),
		vec3(5.0, 7.0, 0.0),
		vec3(1.4),
		LAMBERTIAN | EMISSIVE,
		float(0.4),
		float(0.0)
	},
	{
		vec3(-5.0, 6.99, 2.0),
		vec3(5.0, 7.0, 4.0),
		vec3(1.4),
		LAMBERTIAN | EMISSIVE,
		float(0.4),
		float(0.0)
	},
	// TABLE
	{
		vec3(1.0, 0.0, 1.0),
		vec3(0.8, 2.0, 0.8),
		vec3(0.8),
		METAL,
		float(0.4),
		float(0.0)
	},
	{
		vec3(-1.0, 0.0, 1.0),
		vec3(-0.8, 2.0, 0.8),
		vec3(0.8),
		METAL,
		float(0.4),
		float(0.0)
	},
	{
		vec3(1.0, 0.0, -1.0),
		vec3(0.8, 2.0, -0.8),
		vec3(0.8),
		METAL,
		float(0.4),
		float(0.0)
	},
	{
		vec3(-1.0, 0.0, -1.0),
		vec3(-0.8, 2.0, -0.8),
		vec3(0.8),
		METAL,
		float(0.4),
		float(0.0)
	},
	{
		vec3(-1.2, 2.0, -1.2),
		vec3(1.2, 2.2, 1.2),
		vec3(0.8),
		METAL,
		float(0.4),
		float(0.0)
	},
	// WALLS
	{
		vec3(-5.0, 0.0, -5.0),
		vec3(5.0, 3.0, -5.02),
		vec3(0.8),
		LAMBERTIAN,
		float(0.4),
		float(0.0)
	},
	{
		vec3(5.0, 0.0, -5.0),
		vec3(5.02, 3.0, 5.0),
		vec3(0.8),
		LAMBERTIAN,
		float(0.4),
		float(0.0)
	},
	{
		vec3(5.0, 0.0, 5.0),
		vec3(-5.0, 3.0, 5.02),
		vec3(0.8),
		LAMBERTIAN,
		float(0.4),
		float(0.0)
	},
	{
		vec3(-5.02, 0.0, -5.0),
		vec3(-5.0, 3.0, 5.02),
		vec3(0.8),
		LAMBERTIAN,
		float(0.4),
		float(0.0)
	},
};

const sphere spheres[] = {
    {
        vec3(-2.5, 1.0, 3.0),
        float(1.0),
        vec3(0.9, 0.3, 1.0),
        LAMBERTIAN,
        float(0.0),
        float(1.0)
    },
    {
        vec3(0.0, 1.0, 3.0),
        float(1.0),
        vec3(0.8),
		METAL,
        float(0.0),
        float(1.9)
    },
    {
        vec3(2.5, 1.0, 3.0),
        float(1.0),
        vec3(1.0, 0.8, 0.2),
        DIELECTRIC,
        float(0.0),
        float(1.4)
    }
};

vec2 subpixels[16] = vec2[](
    vec2(-1,     1), vec2(-0.33,     1), vec2(0.33,     1), vec2(1,     1),
    vec2(-1,  0.33), vec2(-0.33,  0.33), vec2(0.33,  0.33), vec2(1,  0.33),
    vec2(-1, -0.33), vec2(-0.33, -0.33), vec2(0.33, -0.33), vec2(1, -0.33),
    vec2(-1,    -1), vec2(-0.33,    -1), vec2(0.33,    -1), vec2(1,    -1)
);

bool intersectBox(vec3 origin, vec3 dir, const box b, out float tNear, out vec3 norm) {
    const float bias = 1.0002;
    vec3 center = (b.min + b.max) * 0.5;
    origin += (origin - center) * (1 - bias);

    vec3 invDir = 1.0 / dir;
    vec3 tMin = (b.min - origin) * invDir;
    vec3 tMax = (b.max - origin) * invDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

	vec3 p = bias * (origin + tNear * dir - center);
	vec3 divisor = (b.min - b.max) * 0.5;
	norm = normalize(vec3(ivec3(p / abs(divisor))));

    return tNear < tFar;
}

bool intersectBoxes(vec3 origin, vec3 dir, out hitinfo info) {
    float smallest = MAX_SCENE_BOUNDS;
    bool found = false;
    for (int i = 0; i < boxes.length; i++) {
        vec3 norm;
        float intersection;
        if (intersectBox(origin, dir, boxes[i], intersection, norm) &&
			intersection > 0.0 &&
            intersection < smallest) {
            info.intersection = intersection;
            info.point = origin + intersection * dir;
            info.normal = norm;
            info.color = boxes[i].color;
            info.material = boxes[i].material;
            info.fuzz = boxes[i].fuzz;
            info.refractionIndex = boxes[i].refractionIndex;
            smallest = intersection;
            found = true;
        }
    }
    return found;
}

bool intersectSphere(vec3 origin, vec3 dir, const sphere s, out hitinfo hit) {
    vec3 oc = origin - s.center;
    float a = dot(dir, dir);
    float b = 2.0 * dot(oc, dir);
    float c = dot(oc, oc) - s.radius * s.radius;
    float det = b*b - 4*a*c;
    if (det > 0) {
        float temp = (-b - sqrt(det)) / (2.0 * a);
        if (temp > 0.0001) {
            hit.intersection = temp;
            hit.point = origin + temp * dir;
            hit.normal = normalize((hit.point - s.center) / s.radius);
            hit.color = s.color;
            hit.material = s.material;
            hit.fuzz = s.fuzz;
            hit.refractionIndex = s.refractionIndex;
            return true;
        }
        temp = (-b + sqrt(det)) / (2.0 * a);
        if (temp > 0.0001) {
            hit.intersection = temp;
            hit.point = origin + temp * dir;
            hit.normal = normalize((hit.point - s.center) / s.radius);
            hit.color = s.color;
            hit.material = s.material;
            hit.fuzz = s.fuzz;
            hit.refractionIndex = s.refractionIndex;
            return true;
        }
    }
    return false;
}

bool intersectSpheres(vec3 origin, vec3 dir, out hitinfo info) {
    float smallest = MAX_SCENE_BOUNDS;
    bool found = false;
    hitinfo hit;
    for (int i = 0; i < spheres.length; i++) {
		sphere sph = spheres[i];
		if (MOTION_BLUR && i == 0) {
			sph.center.z -= rand(vec2(globalTime)) * 0.4;
		}
        if (intersectSphere(origin, dir, sph, hit) &&
            hit.intersection < smallest) {
            info = hit;
            smallest = hit.intersection;
            found = true;
        }
    }
    return found;
}

vec4 backgroundColor() {
	return vec4(vec3(0.0), 1.0);
}

float raytraceShadow(vec3 origin) {
	hitinfo info, hit;
	float depth = MAX_SCENE_BOUNDS;
	vec4 color = backgroundColor();
	atomicCounterIncrement(raysTraced);

	int light = 0;
	int lights = 0;

	for (int i = 0; i < boxes.length(); i++) {
		vec3 boxOrigin = (boxes[i].min + boxes[i].max) / 2.0;
		vec3 dir = boxOrigin - origin;

		if (intersectBoxes(origin, dir, info) &&
			info.intersection < depth) {
			color = vec4(info.color, 1.0);
			depth = info.intersection;
			hit = info;
		}
		if (intersectSpheres(origin, dir, info) &&
			info.intersection < depth) {
			color = vec4(info.color, 1.0);
			depth = info.intersection;
			hit = info;
		}

		if (bool(hit.material & EMISSIVE)) {
			light++;
		}

		if (bool(boxes[i].material & EMISSIVE)) {
			lights++;
		}
	}

	for (int i = 0; i < spheres.length(); i++) {
		//vec3 dir = spheres[i].center - origin;

		//if (intersectSpheres(origin, dir, info) &&
		//	info.intersection < depth) {
		//	if (bool(info.material & EMISSIVE)) {
		//		light += 1.0;
		//	}
		//}

		//if (bool(spheres[i].material & EMISSIVE)) {
		//	lights++;
		//}
	}
	
	return light;
}

vec4 raytrace(vec3 origin, vec3 dir) {
	vec4 c = backgroundColor();
	vec4 bounceColor[8] = { c, c, c, c, c, c, c, c };
	int bouncesMade = 0;

	vec4 color = c;
	hitinfo info, hit;
	for (int bounce = 0; bounce < NUM_BOUNCES; bounce++) {
		atomicCounterIncrement(raysTraced);
		bouncesMade++;

		float depth = MAX_SCENE_BOUNDS;
		bool rayHit = false;
		info = hit;

		if (intersectBoxes(origin, dir, info) &&
			info.intersection < depth) {
			color = vec4(info.color, 1.0);
			depth = info.intersection;
			hit = info;
			rayHit = true;
		}
		if (intersectSpheres(origin, dir, info) &&
			info.intersection < depth) {
			color = vec4(info.color, 1.0);
			depth = info.intersection;
			hit = info;
			rayHit = true;
		}

		if (bounce == NUM_BOUNCES - 1) {
			bounceColor[bounce] = c;
		}

		if (rayHit) {
			bounceColor[bounce] = color;
			if (bool(hit.material & LAMBERTIAN)) {
				origin = hit.point;
				dir = hit.normal + sphereRand(hit.point.xy + globalTime);
			}
			if (bool(hit.material & METAL)) {
				vec3 reflected;
				vec3 spRand = sphereRand(hit.point.xy + globalTime);
				reflected = reflect(normalize(dir), hit.normal) + hit.fuzz * spRand;
				if (dot(reflected, hit.normal) > 0.0) {
					origin = hit.point;
					dir = reflected;
				}
				else {
					break;
				}
			}
			if (bool(hit.material & DIELECTRIC)) {
				bounceColor[bounce] = color;
				dir = normalize(dir);
				vec3 reflected = reflect(dir, hit.normal);
				vec3 normal;
				float eta;
				float reflectProb;
				float cosine;
				if (dot(dir, hit.normal) > 0) {
					normal = -hit.normal;
					eta = hit.refractionIndex;
					cosine = dot(dir, hit.normal);
				}
				else {
					normal = hit.normal;
					eta = 1.0 / hit.refractionIndex;
					cosine = -dot(dir, hit.normal);
				}
				vec3 refracted = refract(dir, normal, eta);
				if (refracted != vec3(0.0)) {
					reflectProb = schlick(cosine, hit.refractionIndex);
				}
				else {
					reflectProb = 1.0;
				}
				if (rand(hit.point.xy + globalTime) < reflectProb) {
					dir = reflected;
				}
				else {
					dir = refracted;
				}
				dir += hit.fuzz * sphereRand(hit.point.xy + globalTime);
				origin = hit.point;
			}
			if (bool(hit.material & EMISSIVE)) {
				break;
			}
		}
		else {
			break;
		}
	}

	for (int bounce = 0; bounce < bouncesMade; bounce++) {
		color *= bounceColor[bounce];
	}

	return color;
}

void main() {
    ivec2 pix = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(framebuffer);
    if (pix.x >= size.x || pix.y >= size.y) {
        return;
    }

	vec4 color = vec4(0.0);
	if (ANTIALIASING) {
		for (int i = 1; i <= MULTISAMPLING_RAYS_EMITTED; i++) {
			vec2 randVec = vec2(rand(pix + i + globalTime), rand(pix * i + globalTime)) * 2.0 - 1.0;

			vec2 jitter = randVec * subpixels[int(rand(vec2(i + globalTime)) * 16)];
			vec2 pos = (vec2(pix) + jitter) / (size.xy - 1);

			vec3 eyeJittered = eye;
			vec3 dir = mix(mix(ray00, ray01, pos.y), mix(ray10, ray11, pos.y), pos.x);

			if (DEPTH_OF_FIELD) {
				vec3 du = length(ray10 - ray00) / size.x * normalize(ray10 - ray00);
				vec3 dv = length(ray01 - ray00) / size.y * normalize(ray01 - ray00);
				eyeJittered += 8 * (randVec.x * du + randVec.y * dv);
				dir         -= 3 * (randVec.x * du + randVec.y * dv);
			}

			color += raytrace(eyeJittered, dir) / MULTISAMPLING_RAYS_EMITTED;
		}
	}
	else {
		vec2 pos = vec2(pix) / vec2(size.x - 1, size.y - 1);
		vec3 dir = mix(mix(ray00, ray01, pos.y), mix(ray10, ray11, pos.y), pos.x);
		color += raytrace(eye, dir);
	}

    vec4 previousColor = imageLoad(framebuffer, pix);
    if (frameCount != 0) {
        color = (previousColor * frameCount + color) / (frameCount + 1);
    }
	if (any(isinf(color)) || any(isnan(color))) {
		return;
	}

    imageStore(framebuffer, pix, color);
}
