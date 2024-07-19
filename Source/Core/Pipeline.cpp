#include "Pipeline.h"

#include "Utils/Random.h"

#include "Object.h"

#include "FpsCamera.h"
#include "Player.h"

#define MAX_SPHERES 16

namespace Simulation {

	float DebugVar = 0.0f;
	int Resolution = 256;

	int Substeps = 3;

	bool DoSim = false;

	bool PhysicsStep = false;

	float* Heightmap;
	Random RandomGen;

	typedef glm::vec3 Force;

	int To1DIdx(int x, int y) {
		return (y * Resolution) + x;
	}

	float Frametime = 0.0f;
	float DeltaTime = 0.0f;
	float CurrentTime;
	bool WireFrame = false;

	Player MainPlayer;
	FPSCamera& Camera = MainPlayer.Camera;

	std::vector<Object> Objects;

	glm::vec3 RSI(glm::vec3 Origin, glm::vec3 Dir, float Radius)
	{
		using namespace glm;
		float VoV = dot(Dir, Dir);
		float Acc = VoV * Radius * Radius;

		// Solve quadratic 
		Acc += 2.0 * Origin.x * dot(glm::vec2(Origin.y, Origin.z), glm::vec2(Dir.y, Dir.z)) * Dir.x;
		Acc += 2.0 * Origin.y * Origin.z * Dir.y * Dir.z;
		Acc -= dot(Origin * Origin, vec3(dot(glm::vec2(Dir.y, Dir.z), glm::vec2(Dir.y, Dir.z)), dot(glm::vec2(Dir.x, Dir.z), glm::vec2(Dir.x, Dir.z)), dot(glm::vec2(Dir.x, Dir.y), glm::vec2(Dir.x, Dir.y))));

		// No intersect 
		if (Acc < 0.0)
		{
			return glm::vec3(-1.0f);
		}

		Acc = sqrt(Acc);

		float Dist1 = (Acc - dot(Origin, Dir)) / VoV;
		float Dist2 = -(Acc + dot(Origin, Dir)) / VoV;

		if (Dist1 >= 0.0 && Dist2 >= 0.0)
		{
			return glm::vec3(Dist1, Dist2, min(Dist1, Dist2));
		}
		else
		{
			return glm::vec3(Dist1, Dist2, max(Dist1, Dist2));
		}
	}

	class RayTracerApp : public Simulation::Application
	{
	public:

		bool vsync;

		RayTracerApp()
		{
			m_Width = 800;
			m_Height = 600;
		}

		void OnUserCreate(double ts) override
		{

		}

		void OnUserUpdate(double ts) override
		{
			glfwSwapInterval((int)vsync);

			GLFWwindow* window = GetWindow();

		}

		void OnImguiRender(double ts) override
		{
			static float r = 0.5f;

			ImGuiIO& io = ImGui::GetIO();
			if (ImGui::Begin("Debug/Edit Mode")) {

				ImGui::SliderFloat("DebugVar", &DebugVar, -1., 1.0f);
				ImGui::NewLine();

				ImGui::Text("Camera Position : %f,  %f,  %f", Camera.GetPosition().x, Camera.GetPosition().y, Camera.GetPosition().z);
				ImGui::Text("Camera Front : %f,  %f,  %f", Camera.GetFront().x, Camera.GetFront().y, Camera.GetFront().z);
				ImGui::Text("Time : %f s", glfwGetTime());

				ImGui::NewLine();
				ImGui::Checkbox("Do Sim", &DoSim);

				PhysicsStep = ImGui::Button("Step Simulation");

				ImGui::SliderInt("Substeps", &Substeps, 1, 100);

				if (ImGui::Button("Mod")) {
					Heightmap[int(RandomGen.Float() * Resolution * Resolution)] = 1.5;
					Heightmap[int(RandomGen.Float() * Resolution * Resolution)] = 0.5f;
				}

				if (ImGui::Button("Reset")) {

					for (int i = 0; i < Resolution * Resolution; i++) {
						Heightmap[i] = 1.f;
					}

				}



				ImGui::NewLine();

				ImGui::Checkbox("Wireframe", &WireFrame);

				ImGui::NewLine();
				ImGui::NewLine();

			} ImGui::End();
		}

		void OnEvent(Simulation::Event e) override
		{
			ImGuiIO& io = ImGui::GetIO();

			if (e.type == Simulation::EventTypes::MousePress && !ImGui::GetIO().WantCaptureMouse && GetCurrentFrame() > 32)
			{

			}

			if (e.type == Simulation::EventTypes::MouseMove && GetCursorLocked())
			{
				Camera.UpdateOnMouseMovement(e.mx, e.my);
			}


			if (e.type == Simulation::EventTypes::MouseScroll && !ImGui::GetIO().WantCaptureMouse)
			{
				float Sign = e.msy < 0.0f ? 1.0f : -1.0f;
				Camera.SetFov(Camera.GetFov() + 2.0f * Sign);
				Camera.SetFov(glm::clamp(Camera.GetFov(), 1.0f, 89.0f));
			}

			if (e.type == Simulation::EventTypes::WindowResize)
			{
				Camera.SetAspect((float)glm::max(e.wx, 1) / (float)glm::max(e.wy, 1));
			}

			if (e.type == Simulation::EventTypes::KeyPress && e.key == GLFW_KEY_ESCAPE) {
				exit(0);
			}

			if (e.type == Simulation::EventTypes::KeyPress && e.key == GLFW_KEY_F1)
			{
				this->SetCursorLocked(!this->GetCursorLocked());
			}

			if (e.type == Simulation::EventTypes::KeyPress && e.key == GLFW_KEY_F2 && this->GetCurrentFrame() > 5)
			{
				Simulation::ShaderManager::RecompileShaders();
			}

			if (e.type == Simulation::EventTypes::KeyPress && e.key == GLFW_KEY_F3 && this->GetCurrentFrame() > 5)
			{
				Simulation::ShaderManager::ForceRecompileShaders();
			}

			if (e.type == Simulation::EventTypes::KeyPress && e.key == GLFW_KEY_V && this->GetCurrentFrame() > 5)
			{
				vsync = !vsync;
			}



		}


	};

	float SampleHeight(int x, int y) {
		return Heightmap[To1DIdx(x, y)];
	}


	float SampleHeightClamped(int x, int y) {
		x = glm::clamp(x, 0, Resolution - 1);
		y = glm::clamp(y, 0, Resolution - 1);
		return SampleHeight(x, y);
	}

	float SampleHeightClamped(glm::ivec2 t) {
		t.x = glm::clamp(t.x, 0, Resolution - 1);
		t.y = glm::clamp(t.y, 0, Resolution - 1);
		return SampleHeight(t.x, t.y);
	}


	void Pipeline::StartPipeline()
	{
		// Application
		RayTracerApp app;
		app.Initialize();
		app.SetCursorLocked(false);

		// Create VBO and VAO for drawing the screen-sized quad.
		GLClasses::VertexBuffer ScreenQuadVBO;
		GLClasses::VertexArray ScreenQuadVAO;

		RandomGen.Float();

		//GLClasses::Texture Heightmap;
		//Heightmap.CreateTexture("Res/Heightmap.png", false, false, false, GL_TEXTURE_2D,
		//	GL_LINEAR, GL_LINEAR,
		//	GL_REPEAT, GL_REPEAT, false);

		// Setup screensized quad for rendering
		{
			float QuadVertices_NDC[] =
			{
				-1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,
				 1.0f, -1.0f,  1.0f, 0.0f, -1.0f,  1.0f,  0.0f, 1.0f,
				 1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
			};

			ScreenQuadVBO.Bind();
			ScreenQuadVBO.BufferData(sizeof(QuadVertices_NDC), QuadVertices_NDC, GL_STATIC_DRAW);

			ScreenQuadVAO.Bind();
			ScreenQuadVBO.Bind();
			ScreenQuadVBO.VertexAttribPointer(0, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), 0);
			ScreenQuadVBO.VertexAttribPointer(1, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
			ScreenQuadVAO.Unbind();
		}
		// Create Shaders 
		ShaderManager::CreateShaders();

		// Shaders
		GLClasses::Shader& BlitShader = ShaderManager::GetShader("BLIT");
		GLClasses::Shader& RTSphere = ShaderManager::GetShader("RD");

		GLClasses::Framebuffer GBuffer = GLClasses::Framebuffer(16, 16, { {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false},  {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false} }, true, true);

		// Create Heightmaps
		Heightmap = new float[Resolution * Resolution];

		memset(Heightmap, 0, Resolution * Resolution * sizeof(float));

		// GPU Data
		GLuint HeightmapSSBO = 0;
		glGenBuffers(1, &HeightmapSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, HeightmapSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * Resolution * Resolution, (void*)0, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// For now.
		for (int x = 0; x < Resolution; x++) {
			for (int y = 0; y < Resolution; y++) {
				int i = To1DIdx(x, y);
				Heightmap[i] = 1.0f;
			}
		}
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		while (!glfwWindowShouldClose(app.GetWindow())) {


			glDisable(GL_CULL_FACE);

			app.OnUpdate();

			// FBO Update
			GBuffer.SetSize(app.GetWidth(), app.GetHeight());

			// Player
			MainPlayer.OnUpdate(app.GetWindow(), DeltaTime, 0.5f, app.GetCurrentFrame());

			// SIMULATE
			if (DoSim || PhysicsStep)
			{
				PhysicsStep = false;
			}

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, HeightmapSSBO);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * Resolution * Resolution, Heightmap, GL_DYNAMIC_DRAW);

			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT, GL_FILL);
			glPolygonMode(GL_BACK, GL_FILL);

			// Sphere

			GBuffer.Bind();
			RTSphere.Use();

			RTSphere.SetInteger("u_Texture", 0);
			RTSphere.SetInteger("u_Depth", 1);

			RTSphere.SetFloat("u_zNear", Camera.GetNearPlane());
			RTSphere.SetFloat("u_zFar", Camera.GetFarPlane());
			RTSphere.SetMatrix4("u_InverseProjection", glm::inverse(Camera.GetProjectionMatrix()));
			RTSphere.SetMatrix4("u_InverseView", glm::inverse(Camera.GetViewMatrix()));

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			// Blit

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glPolygonMode(GL_FRONT, GL_FILL);
			glPolygonMode(GL_BACK, GL_FILL);

			BlitShader.Use();

			BlitShader.SetInteger("u_Texture", 0);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture());

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			glFinish();
			app.FinishFrame();

			CurrentTime = glfwGetTime();
			DeltaTime = CurrentTime - Frametime;
			Frametime = glfwGetTime();

			GLClasses::DisplayFrameRate(app.GetWindow(), "Simulation ");
		}
	}
}