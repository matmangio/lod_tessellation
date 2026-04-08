#version 410 core

// UNIFORMS
// ambient, diffusive and specular components (passed from the application)
uniform vec3 ambient_color;
uniform vec3 diffuse_color;
uniform vec3 specular_color;
// weight of the components
// in this case, we can pass separate values from the main application even if Ka+Kd+Ks>1. In more "realistic" situations, I have to set this sum = 1, or at least Kd+Ks = 1, by passing Kd as uniform, and then setting Ks = 1.0-Kd
uniform float k_a;
uniform float k_d;
uniform float k_s;

// shininess coefficients (passed from the application)
uniform float shininess;

// INPUTS
in vec3 N;
in vec3 L;
in vec3 V;

// OUTPUTS
out vec4 frag_color;

vec3 blinn_phong() // this name is the one which is detected by the SetupShaders() function in the main application, and the one used to swap subroutines
{
    // ambient component can be calculated at the beginning
    vec3 color = k_a * ambient_color;

    // normalization of the per-fragment normal
    vec3 N = normalize(N);

    // normalization of the per-fragment light incidence direction
    vec3 L = normalize(L);

    // Lambert coefficient
    float lambertian = max(dot(L, N), 0.0);

    // if the lambert coefficient is positive, then I can calculate the specular component
    if(lambertian > 0.0)
    {
      // the view vector has been calculated in the vertex shader, already negated to have direction from the mesh to the camera
      vec3 V = normalize(V);

      // in the Blinn-Phong model we do not use the reflection vector, but the half vector
      vec3 H = normalize(L + V);

      // we use H to calculate the specular component
      float spec_angle = max(dot(H, N), 0.0);
      // shininess application to the specular component
      float specular = pow(spec_angle, shininess);

      // We add diffusive and specular components to the final color
      // N.B. ): in this implementation, the sum of the components can be different than 1
      color += vec3( k_d * lambertian * diffuse_color +
                      k_s * specular * specular_color);
    }
    return color;
}

void main() {
    vec3 color = blinn_phong();
	frag_color = vec4(color, 1.0);
}
