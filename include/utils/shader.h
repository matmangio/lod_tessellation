/*
Shader class
- loading Shader source code, Shader Program creation

N.B. ) adaptation of https://github.com/JoeyDeVries/LearnOpenGL/blob/master/includes/learnopengl/shader.h

author: Davide Gadia

Real-Time Graphics Programming - a.a. 2024/2025
Master degree in Computer Science
Universita' degli Studi di Milano
*/

#pragma once

using namespace std;

// Std. Includes
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <glad/glad.h>

/////////////////// SHADER class ///////////////////////
class Shader
{
public:
    GLuint Program;

    //////////////////////////////////////////

    //constructor
    Shader(const GLchar* vertexPath, const GLchar* fragmentPath, const GLchar* tessCtrlPath = NULL, const GLchar* tessEvalPath = NULL)
    {
        // Step 1: we retrieve shaders source code from provided filepaths
		string vertexCode, fragmentCode, tessCtrlCode, tessEvalCode;
        ifstream vShaderFile, fShaderFile, tcShaderFile, teShaderFile;

        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions (ifstream::failbit | ifstream::badbit);
		fShaderFile.exceptions (ifstream::failbit | ifstream::badbit);
		teShaderFile.exceptions (ifstream::failbit | ifstream::badbit);
		tcShaderFile.exceptions (ifstream::failbit | ifstream::badbit);
        try
        {
            // Open files
            vShaderFile.open(vertexPath);
			fShaderFile.open(fragmentPath);
			if (tessCtrlPath != NULL) tcShaderFile.open(tessCtrlPath);
			if (tessEvalPath != NULL) teShaderFile.open(tessEvalPath);

			// Read file's buffer contents into streams
            stringstream vShaderStream, fShaderStream, tcShaderStream, teShaderStream;
            vShaderStream << vShaderFile.rdbuf();
			fShaderStream << fShaderFile.rdbuf();
			if (tessCtrlPath != NULL) tcShaderStream << tcShaderFile.rdbuf();
			if (tessEvalPath != NULL) teShaderStream << teShaderFile.rdbuf();

            // close file handlers
            vShaderFile.close();
			fShaderFile.close();
			if (tessCtrlPath != NULL) tcShaderFile.close();
			if (tessEvalPath != NULL) teShaderFile.close();

            // Convert stream into string
            vertexCode = vShaderStream.str();
			fragmentCode = fShaderStream.str();
			if (tessCtrlPath != NULL) tessCtrlCode = tcShaderStream.str();
			if (tessEvalPath != NULL) tessEvalCode = teShaderStream.str();
        }
        catch (ifstream::failure const&)
        {
            cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << endl;
        }

		const GLchar* vShaderCode = vertexCode.c_str();
        const GLchar* fShaderCode = fragmentCode.c_str();
		const GLchar* tcShaderCode = tessCtrlCode.c_str();
		const GLchar* teShaderCode = tessEvalCode.c_str();

        // Step 2: we compile the shaders
        GLuint vertex, fragment, tessControl, tessEvaluation;

        // Vertex Shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        // check compilation errors
        checkCompileErrors(vertex, "VERTEX");

		// If present, do the same for tessellation shaders
		if (tessCtrlPath != NULL) {
        	tessControl = glCreateShader(GL_TESS_CONTROL_SHADER);
			glShaderSource(tessControl, 1, &tcShaderCode, NULL);
			glCompileShader(tessControl);
			checkCompileErrors(tessControl, "TESSELLATION CONTROL");
		}

		if (tessEvalPath != NULL) {
        	tessEvaluation = glCreateShader(GL_TESS_EVALUATION_SHADER);
			glShaderSource(tessEvaluation, 1, &teShaderCode, NULL);
			glCompileShader(tessEvaluation);
			checkCompileErrors(tessEvaluation, "TESSELLATION EVALUATION");
		}

		// Fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        // check compilation errors
        checkCompileErrors(fragment, "FRAGMENT");

        // Step 3: Shader Program creation
        this->Program = glCreateProgram();
        glAttachShader(this->Program, vertex);
		if (tessCtrlPath != NULL) glAttachShader(this->Program, tessControl);
		if (tessEvalPath != NULL) glAttachShader(this->Program, tessEvaluation);
		glAttachShader(this->Program, fragment);
        glLinkProgram(this->Program);
        // check linking errors
        checkCompileErrors(this->Program, "PROGRAM");

        // Step 4: we delete the shaders because they are linked to the Shader Program, and we do not need them anymore
        glDeleteShader(vertex);
        glDeleteShader(fragment);
		if (tessCtrlPath != NULL) glDeleteShader(tessControl);
		if (tessEvalPath != NULL) glDeleteShader(tessEvaluation);
    }

    //////////////////////////////////////////

    // We activate the Shader Program as part of the current rendering process
    void Use() { glUseProgram(this->Program); }

    // We delete the Shader Program when application closes
    void Delete() { glDeleteProgram(this->Program); }

private:
    //////////////////////////////////////////

    // Check compilation and linking errors
    void checkCompileErrors(GLuint shader, string type)
	{
		GLint success;
		GLchar infoLog[1024];
		if(type != "PROGRAM")
		{
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if(!success)
			{
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                cout << "| ERROR::::SHADER-COMPILATION-ERROR of type: " << type << "|\n" << infoLog << "\n| -- --------------------------------------------------- -- |" << endl;
			}
		}
		else
		{
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if(!success)
			{
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                cout << "| ERROR::::PROGRAM-LINKING-ERROR of type: " << type << "|\n" << infoLog << "\n| -- --------------------------------------------------- -- |" << endl;
			}
		}
	}

	const GLchar* read_shader_from_file(const GLchar* path) {
		string code;
        ifstream shaderFile;

        // ensure ifstream objects can throw exceptions:
        shaderFile.exceptions (ifstream::failbit | ifstream::badbit);
        try
        {
            // Open files
            shaderFile.open(path);
            stringstream shaderStream;
            // Read file's buffer contents into streams
            shaderStream << shaderFile.rdbuf();
            // close file handlers
            shaderFile.close();
            // Convert stream into string
            code = shaderStream.str();
        }
        catch (ifstream::failure const&)
        {
            cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << endl;
        }

		return code.c_str();
	}
};
