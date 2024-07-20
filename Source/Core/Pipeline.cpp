#include "Pipeline.h"

#include "Utils/Random.h"

#include "Object.h"

#include "FpsCamera.h"
#include "Player.h"

#define MAX_SPHERES 16

namespace Simulation {

	/*
	1) Verlet Acceleration
	2) Projection to maintain incompressability 
	3) Advection
	*/

	enum Directions : uint8_t {
		UP=0, 
		DOWN,
		LEFT,
		RIGHT
	};
	
	// Optimal to use memory complexity 2(n+2)^2 instead of 4n^2 for n >= 5
	struct Cell {

		// 0 -> Right Velocity
		// 1 -> Bottom Velocity
		glm::vec2 Velocities;
	};

	// Boiler
	typedef glm::vec3 Force;
	// Boiler

	// Simulation

	float GridSpacing = 1.;
	float DensityWater = 1000.0f;
	float OverRelaxationCoefficient = 1.0f;

	const int SimulationMapResolution = 256;
	const int PaddedResolution = SimulationMapResolution + 2;
	Cell* SimulationMap;
	float* PressureGrid;
	float Gravity = 9.81f; 


	float DebugVar = 0.0f;

	// Sim
	int Substeps = 3;
	bool DoSim = false;
	bool PhysicsStep = false;

	// RNG 
	Random RandomGen;

	// Conversion Functions 
	int To1DIdxMap(int x, int y) {
		++x;
		++y;
		return (y * PaddedResolution) + x;
	}

	int To1DIdx(int x, int y) {
		return (y * SimulationMapResolution) + x;
	}


	bool IsObstacle(int x, int y, Directions dir) {

		const glm::ivec2 Offsets[4] = {
			glm::ivec2(0,1), glm::ivec2(0,-1), glm::ivec2(-1,0), glm::ivec2(1,0)
		};

		if (x < 0 || x >= SimulationMapResolution || y < 0 || y >= SimulationMapResolution) {
			return true;
		}

		return false;
	}

	// Gets velocity at a particular direction
	// Assume velocity is at the border of a square 
	float GetVelocity(int x, int y, Directions dir) {
		const glm::ivec3 References[4] = {
			  glm::ivec3(0,1,1),
			  glm::ivec3(0,0,1),
			  glm::ivec3(-1,0,0),
			  glm::ivec3(0,0,0)
		};

		if (int(dir) < 0 || int(dir) > 3) {
			throw "WTFFF";
		}

		const auto& r = References[int(dir)];
		return SimulationMap[To1DIdxMap(x + r.x, y + r.y)].Velocities[r.z];
	}

	float& GetVelocityRef(int x, int y, Directions dir) {
		const glm::ivec3 References[4] = {
			  glm::ivec3(0,1,1),
			  glm::ivec3(0,0,1),
			  glm::ivec3(-1,0,0),
			  glm::ivec3(0,0,0)
		};
		if (int(dir) < 0 || int(dir) > 3) {
			throw "WTFFF";
		}

		glm::ivec3 r = References[int(dir)];
		return SimulationMap[To1DIdxMap(x + r.x, y + r.y)].Velocities[r.z];
	}

	float GetDirectionSign(Directions dir) {
		if (dir == Directions::DOWN || dir == Directions::LEFT) {
			return -1.;
		}
		return 1.;
	}

	float Frametime = 0.0f;
	float DeltaTime = 0.0f;
	float CurrentTime;
	bool WireFrame = false;

	Player MainPlayer;
	FPSCamera& Camera = MainPlayer.Camera;

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

				if (ImGui::Button("Reset")) {
					memset(SimulationMap, 0, PaddedResolution * PaddedResolution * sizeof(Cell));
					memset(PressureGrid, 0, SimulationMapResolution * SimulationMapResolution * sizeof(float));
				}



				ImGui::NewLine();

				ImGui::SliderFloat("Grid Spacing", &GridSpacing, 0.0f, 10.0f);
				ImGui::SliderFloat("Density Water", &DensityWater, 10.0f, 10000.0f);
				ImGui::SliderFloat("Over Relaxation Coeff", &OverRelaxationCoefficient, 0.0f, 2.0f);

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

	// Account for gravity 
	void SimulateVelocities(float dt) {

		bool ObstacleCache[4];

		memset(ObstacleCache, 0, 4 * sizeof(bool));

		for (int x = 0; x < SimulationMapResolution; x++) {
			for (int y = 0; y < SimulationMapResolution; y++) {
				float Weight = 0.;

				for (uint8_t z = 0; z < 4; z++) {
					float& v = GetVelocityRef(x, y, Directions(z));
					ObstacleCache[z] = IsObstacle(x, y, Directions(z));

					if (!ObstacleCache[z]) {
						v += Gravity * dt * -1.;
					}
					
					Weight += float(!ObstacleCache[z]);
				}

				if (Weight < 0.01f) {
					continue;
				}

				// Handle divergance 
				float Divergance = 0.0f;

				Divergance = OverRelaxationCoefficient * (GetVelocity(x, y, Directions::RIGHT) - GetVelocity(x, y, Directions::LEFT));
				Divergance += OverRelaxationCoefficient* (GetVelocity(x, y, Directions::UP) - GetVelocity(x, y, Directions::DOWN));

				// For divergance > 0, too much outflow
				// For divergance < 0, too much inflow
				// For divergance = 0, it is a perfectly incompressible surface 
				// WE need to make the divergance zero

				float PushAmount = Divergance / Weight;
				float Avg = 0.0f;

				// Gauss Seidel method 
				for (uint8_t z = 0; z < 4; z++) {

					if (!ObstacleCache[int(z)]) {

						float& v = GetVelocityRef(x, y, Directions(z));
						v += PushAmount * float(!ObstacleCache[z]) * GetDirectionSign(Directions(z)) * -1.;
						Avg += v;
					}
				}

				// Solve for pressure gradient 

				// Todo : WHY divergance/weight?
				// Essence? WHY.?
				
				Avg /= 4.;
				PressureGrid[To1DIdx(x, y)] = (Divergance / Weight)* (DensityWater * GridSpacing / DeltaTime);
				
			}
		}

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
		GLClasses::Shader& RenderShader = ShaderManager::GetShader("RD");
		GLClasses::Framebuffer GBuffer = GLClasses::Framebuffer(16, 16, { {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false},  {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false} }, true, true);

		// CPU data
		SimulationMap = new Cell[PaddedResolution * PaddedResolution];
		memset(SimulationMap, 0, PaddedResolution * PaddedResolution * sizeof(Cell));

		PressureGrid = new float[SimulationMapResolution * SimulationMapResolution];
		memset(PressureGrid, 0, SimulationMapResolution * SimulationMapResolution * sizeof(float));


		for (int x = 0; x < SimulationMapResolution; x++) {
			for (int y = 0; y < SimulationMapResolution; y++) {
				
				glm::vec2 V = glm::vec2(x, y);
				V /= float(SimulationMapResolution);
				V = V * 2.f - 1.f;

				float d = glm::distance(V, glm::vec2(0.0));
				
				for (int z = 0; z < 4; z++) {
					auto& v = GetVelocityRef(x, y, Directions(z));
					v = 0.0f;//RandomGen.Float();

				}

				for (int z = 0; z < 4; z++) {
					auto& v = GetVelocityRef(x, y, Directions(z));

					if (d < 0.7f)
						v = 10.0f;//RandomGen.Float();

				}
			}
		}

		// GPU Data
		GLuint PressureGradientSSBO = 0;
		glGenBuffers(1, &PressureGradientSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, PressureGradientSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * SimulationMapResolution * SimulationMapResolution, (void*)0, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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
				SimulateVelocities(DeltaTime);
				PhysicsStep = false;
			}

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, PressureGradientSSBO);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * SimulationMapResolution * SimulationMapResolution, PressureGrid, GL_DYNAMIC_DRAW);

			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT, GL_FILL);
			glPolygonMode(GL_BACK, GL_FILL);

			// Sphere

			GBuffer.Bind();

			RenderShader.Use();

			RenderShader.SetInteger("u_Resolution", (SimulationMapResolution));
			RenderShader.SetFloat("u_zNear", Camera.GetNearPlane());
			RenderShader.SetFloat("u_zFar", Camera.GetFarPlane());
			RenderShader.SetMatrix4("u_InverseProjection", glm::inverse(Camera.GetProjectionMatrix()));
			RenderShader.SetMatrix4("u_InverseView", glm::inverse(Camera.GetViewMatrix()));

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, PressureGradientSSBO);

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