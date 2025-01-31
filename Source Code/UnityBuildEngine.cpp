#include "stdafx.h"


#if defined(CEGUIGLRENDERER)
#include "Libs\CEGUIRenderer\OpenGL\src\GeometryBufferBase.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\GL.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\GL3FBOTextureTarget.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\GL3GeometryBuffer.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\GL3Renderer.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\RendererBase.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\Shader.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\ShaderManager.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\StateChangeWrapper.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\Texture.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\TextureTarget.cpp"
#include "Libs\CEGUIRenderer\OpenGL\src\ViewportTarget.cpp"
#elif defined(CORE)
#include "Libs\ArenaAllocator\arena_allocator.cpp"
#include "Core\Debugging\DebugInterface.cpp"
#include "Core\Math\BoundingVolumes\BoundingBox.cpp"
#include "Core\Math\BoundingVolumes\BoundingSphere.cpp"
#include "Core\Math\MathClasses.cpp"
#include "Core\Math\MathHelper.cpp"
#include "Core\Math\Transform.cpp"
#include "Core\Resources\ConcreteLoaders\AudioDescriptorLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\Box3DLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\ImpostorLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\LightLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\MaterialLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\MeshLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\Quad3DLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\ShaderProgramLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\Sphere3DLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\SubMeshLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\TextureLoaderImpl.cpp"
#include "Core\Resources\Resource.cpp"
#include "Core\Resources\ResourceCache.cpp"
#include "Core\Resources\ResourceDescriptor.cpp"
#include "Core\Time\ApplicationTimer.cpp"
#include "Core\Time\FrameRateHandler.cpp"
#include "Core\Time\ProfileTimer.cpp"
#include "Core\ByteBuffer.cpp"
#include "Core\Console.cpp"
#include "Core\GUIDWrapper.cpp"
#include "Core\RingBuffer.cpp"
#include "Core\StringHelper.cpp"
#include "Core\TaskPool.cpp"
#include "Core\WindowManager.cpp"
#include "Libs\nv_dds\nv_dds.cpp"
#include "Libs\SimpleINI\src\ConvertUTF.cpp"
#include "Utility\CommandParser.cpp"
#include "Utility\CRC.cpp"
#include "Utility\EASTLImport.cpp"
#include "Utility\ImageTools.cpp"
#include "Utility\Localization.cpp"
#include "Utility\TextLabel.cpp"
#include "Utility\XMLParser.cpp"
#elif defined(WORLDEDITOR)
#include "Libs\imgui\addons\imguigizmo\ImGuizmo.cpp"
#include "Libs\imgui\addons\imguigizmo\ImSequencer.cpp"
#include "Libs\imgui\addons\imguiyesaddons\imguiimageeditor_plugins\jo_gif.cpp"
#include "Libs\imgui\addons\imguiyesaddons\imguiimageeditor_plugins\lodepng.cpp"
#include "Libs\imgui\extra_fonts\binary_to_compressed_c.cpp"
#include "Libs\STB\stb_vorbis.c"
#include "Libs\imgui\imgui.cpp"
#include "Libs\imgui\imgui_demo.cpp"
#include "Libs\imgui\imgui_draw.cpp"
#include "Editor\Widgets\DockedWindows\ApplicationOutputWindow.cpp"
#include "Editor\Widgets\DockedWindows\FindWindow.cpp"
#include "Editor\Widgets\DockedWindows\OutputWindow.cpp"
#include "Editor\Widgets\DockedWindows\PreferencesWindow.cpp"
#include "Editor\Widgets\DockedWindows\PropertyWindow.cpp"
#include "Editor\Widgets\DockedWindows\SolutionExplorerWindow.cpp"
#include "Editor\Widgets\DockedWindows\ToolboxWindow.cpp"
#include "Editor\Widgets\ApplicationOutput.cpp"
#include "Editor\Widgets\DockedWindow.cpp"
#include "Editor\Widgets\MenuBar.cpp"
#include "Editor\Widgets\PanelManager.cpp"
#include "Editor\Widgets\PanelManagerPane.cpp"
#include "Editor\Widgets\TabbedWindow.cpp"
#include "Editor\Editor.cpp"
#include "Editor\ImguiExtras.cpp"
#elif defined(ENGINE)
#include "Core\Networking\LocalClient.cpp"
#include "Core\Networking\Patch.cpp"
#include "Core\Networking\Server.cpp"
#include "Core\Networking\Session.cpp"
#include "Core\Application.cpp"
#include "Core\Configuration.cpp"
#include "Core\EngineTaskPool.cpp"
#include "Core\Kernel.cpp"
#include "Core\PlatformContext.cpp"
#include "Core\XMLEntryData.cpp"
#include "ECS\Components\AnimationComponent.cpp"
#include "ECS\Components\BoundsComponent.cpp"
#include "ECS\Components\EditorComponent.cpp"
#include "ECS\Components\IKComponent.cpp"
#include "ECS\Components\NavigationComponent.cpp"
#include "ECS\Components\NetworkingComponent.cpp"
#include "ECS\Components\RagdollComponent.cpp"
#include "ECS\Components\RenderingComponent.cpp"
#include "ECS\Components\RenderingComponentState.cpp"
#include "ECS\Components\RigidBodyComponent.cpp"
#include "ECS\Components\TransformComponent.cpp"
#include "ECS\Components\UnitComponent.cpp"
#include "Libs\EntityComponentSystem\src\Event\EventHandler.cpp"
#include "Libs\EntityComponentSystem\src\Event\IEvent.cpp"
#include "Libs\EntityComponentSystem\src\Event\IEventListener.cpp"
#include "Libs\EntityComponentSystem\src\Log\Logger.cpp"
#include "Libs\EntityComponentSystem\src\Log\LoggerManager.cpp"
#include "Libs\EntityComponentSystem\src\Memory\Allocator\IAllocator.cpp"
#include "Libs\EntityComponentSystem\src\Memory\Allocator\LinearAllocator.cpp"
#include "Libs\EntityComponentSystem\src\Memory\Allocator\PoolAllocator.cpp"
#include "Libs\EntityComponentSystem\src\Memory\Allocator\StackAllocator.cpp"
#include "Libs\EntityComponentSystem\src\Memory\ECSMM.cpp"
#include "Libs\EntityComponentSystem\src\util\FamilyTypeID.cpp"
#include "Libs\EntityComponentSystem\src\util\Timer.cpp"
#include "Libs\EntityComponentSystem\src\API.cpp"
#include "Libs\EntityComponentSystem\src\ComponentManager.cpp"
#include "Libs\EntityComponentSystem\src\Engine.cpp"
#include "Libs\EntityComponentSystem\src\EntityManager.cpp"
#include "Libs\EntityComponentSystem\src\IComponent.cpp"
#include "Libs\EntityComponentSystem\src\IEntity.cpp"
#include "Libs\EntityComponentSystem\src\ISystem.cpp"
#include "Libs\EntityComponentSystem\src\SystemManager.cpp"
#include "ECS\Systems\AnimationSystem.cpp"
#include "ECS\Systems\BoundsSystem.cpp"
#include "ECS\Systems\ECSManager.cpp"
#include "ECS\Systems\RenderingSystem.cpp"
#include "ECS\Systems\TransformSystem.cpp"
#include "ECS\Systems\UpdateSystem.cpp"
#include "Geometry\Animations\AnimationEvaluator.cpp"
#include "Geometry\Animations\AnimationUtils.cpp"
#include "Geometry\Animations\SceneAnimator.cpp"
#include "Geometry\Animations\SceneAnimatorFileIO.cpp"
#include "Geometry\Importer\DVDConverter.cpp"
#include "Geometry\Importer\MeshImporter.cpp"
#include "Geometry\Material\Material.cpp"
#include "Geometry\Material\ShaderComputeQueue.cpp"
#include "Geometry\Material\ShaderProgramInfo.cpp"
#include "Geometry\Shapes\Predefined\Box3D.cpp"
#include "Geometry\Shapes\Predefined\Quad3D.cpp"
#include "Geometry\Shapes\Predefined\Sphere3D.cpp"
#include "Geometry\Shapes\Mesh.cpp"
#include "Geometry\Shapes\Object3D.cpp"
#include "Geometry\Shapes\SkinnedSubMesh.cpp"
#include "Geometry\Shapes\SubMesh.cpp"
#include "Graphs\IntersectionRecord.cpp"
#include "Graphs\Octree.cpp"
#include "Graphs\SceneGraph.cpp"
#include "Graphs\SceneGraphNode.cpp"
#include "Graphs\SceneNode.cpp"
#include "Graphs\SceneNodeRenderState.cpp"
#include "Graphs\SGNRelationshipCache.cpp"
#include "GUI\CEGUIAddons\CEGUIFormattedListBox.cpp"
#include "GUI\CEGUIAddons\CEGUIInput.cpp"
#include "GUI\GUI.cpp"
#include "GUI\GUIButton.cpp"
#include "GUI\GUIConsole.cpp"
#include "GUI\GUIConsoleCommandParser.cpp"
#include "GUI\GUIElement.cpp"
#include "GUI\GuiFlash.cpp"
#include "GUI\GUIInterface.cpp"
#include "GUI\GUIMessageBox.cpp"
#include "GUI\GUISplash.cpp"
#include "GUI\GUIText.cpp"
#include "Managers\FrameListenerManager.cpp"
#include "Managers\RenderPassManager.cpp"
#include "Managers\SceneManager.cpp"
#include "Physics\PhysX\PhysX.cpp"
#include "Physics\PhysX\PhysXActor.cpp"
#include "Physics\PhysX\PhysXSceneInterface.cpp"
#include "Physics\PhysX\pxShapeScaling.cpp"
#include "Physics\PhysicsAsset.cpp"
#include "Physics\PhysicsSceneInterface.cpp"
#include "Physics\PXDevice.cpp"
#include "Rendering\Camera\Camera.cpp"
#include "Rendering\Camera\CameraPool.cpp"
#include "Rendering\Camera\FirstPersonCamera.cpp"
#include "Rendering\Camera\FreeFlyCamera.cpp"
#include "Rendering\Camera\Frustum.cpp"
#include "Rendering\Camera\OrbitCamera.cpp"
#include "Rendering\Camera\ScriptedCamera.cpp"
#include "Rendering\Camera\ThirdPersonCamera.cpp"
#include "Rendering\Lighting\ShadowMapping\CascadedShadowMaps.cpp"
#include "Rendering\Lighting\ShadowMapping\CubeShadowMap.cpp"
#include "Rendering\Lighting\ShadowMapping\ShadowMap.cpp"
#include "Rendering\Lighting\ShadowMapping\SingleShadowMap.cpp"
#include "Rendering\Lighting\DirectionalLight.cpp"
#include "Rendering\Lighting\Light.cpp"
#include "Rendering\Lighting\LightPool.cpp"
#include "Rendering\Lighting\PointLight.cpp"
#include "Rendering\Lighting\SpotLight.cpp"
#include "Rendering\PostFX\CustomOperators\BloomPreRenderOperator.cpp"
#include "Rendering\PostFX\CustomOperators\DoFPreRenderOperator.cpp"
#include "Rendering\PostFX\CustomOperators\PostAAPreRenderOperator.cpp"
#include "Rendering\PostFX\CustomOperators\SSAOPreRenderOperator.cpp"
#include "Rendering\PostFX\PostFX.cpp"
#include "Rendering\PostFX\PreRenderBatch.cpp"
#include "Rendering\PostFX\PreRenderOperator.cpp"
#include "Rendering\RenderPass\RenderBin.cpp"
#include "Rendering\RenderPass\RenderPass.cpp"
#include "Rendering\RenderPass\RenderPassCuller.cpp"
#include "Rendering\RenderPass\RenderQueue.cpp"
#include "Rendering\Renderer.cpp"
#include "Rendering\EnvironmentProbe.cpp"
#include "Scripting\GameScript.cpp"
#include "Scripting\Script.cpp"
#include "Scripting\ScriptBindings.cpp"
#include "engineMain.cpp"
#elif defined(EXECUTABLE)
//#include "HotReloading\HotReloadingApple.cpp"
//#include "HotReloading\HotReloadingUnix.cpp"
//#include "HotReloading\HotReloadingWindows.cpp"
#include "main.cpp"
#elif defined(GAME)
#include "Libs\CPPGoap\Action.cpp"
#include "Libs\CPPGoap\Node.cpp"
#include "Libs\CPPGoap\Planner.cpp"
#include "Libs\CPPGoap\WorldState.cpp"
#include "AI\ActionInterface\AIProcessor.cpp"
#include "AI\ActionInterface\AITeam.cpp"
#include "AI\ActionInterface\GOAPInterface.cpp"
#include "AI\PathFinding\NavMeshes\NavMesh.cpp"
#include "AI\PathFinding\NavMeshes\NavMeshContext.cpp"
#include "AI\PathFinding\NavMeshes\NavMeshDebugDraw.cpp"
#include "AI\PathFinding\NavMeshes\NavMeshLoader.cpp"
#include "AI\PathFinding\Waypoints\Waypoint.cpp"
#include "AI\PathFinding\Waypoints\WaypointGraph.cpp"
#include "AI\PathFinding\DivideCrowd.cpp"
#include "AI\PathFinding\DivideRecast.cpp"
#include "AI\Sensors\AudioSensor.cpp"
#include "AI\Sensors\VisualSensor.cpp"
#include "AI\AIEntity.cpp"
#include "AI\AIManager.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleBoxGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleColourGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleRoundGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleSphereVelocityGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleTimeGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleVelocityFromPositionGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteGenerators\ParticleVelocityGenerator.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleAttractorUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleBasicColourUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleBasicTimeUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleEulerUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleFloorUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleFountainUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticlePositionColourUpdater.cpp"
#include "Dynamics\Entities\Particles\ConcreteUpdaters\ParticleVelocityColourUpdater.cpp"
#include "Dynamics\Entities\Particles\ParticleData.cpp"
#include "Dynamics\Entities\Particles\ParticleEmitter.cpp"
#include "Dynamics\Entities\Particles\ParticleGenerator.cpp"
#include "Dynamics\Entities\Particles\ParticleSource.cpp"
#include "Dynamics\Entities\Triggers\Trigger.cpp"
#include "Dynamics\Entities\Units\Character.cpp"
#include "Dynamics\Entities\Units\NPC.cpp"
#include "Dynamics\Entities\Units\Player.cpp"
#include "Dynamics\Entities\Units\Unit.cpp"
#include "Dynamics\Entities\Units\Vehicle.cpp"
#include "Dynamics\Entities\Impostor.cpp"
#include "Dynamics\WeaponSystem\Ammunition\Ammunition.cpp"
#include "Dynamics\WeaponSystem\Projectile\Projectile.cpp"
#include "Dynamics\WeaponSystem\Weapons\Weapon.cpp"
#include "Environment\Sky\Sky.cpp"
#include "Environment\Terrain\Terrain.cpp"
#include "Environment\Terrain\TerrainLoader.cpp"
#include "Environment\Terrain\TerrainTessellator.cpp"
#include "Environment\Vegetation\Vegetation.cpp"
#include "Environment\Water\Water.cpp"
#include "GUI\SceneGUIElements.cpp"
#include "Core\Resources\ConcreteLoaders\ParticleEmitterLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\SkyLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\TerrainLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\TriggerLoaderImpl.cpp"
#include "Core\Resources\ConcreteLoaders\WaterPlaneLoaderImpl.cpp"
#include "Scenes\DefaultScene\DefaultScene.cpp"
#include "Scenes\MainScene\MainScene.cpp"
#include "Scenes\PingPongScene\PingPongScene.cpp"
#include "Scenes\WarScene\AESOPActions\WarSceneActions.cpp"
#include "Scenes\WarScene\WarScene.cpp"
#include "Scenes\WarScene\WarSceneAI.cpp"
#include "Scenes\WarScene\WarSceneAIProcessor.cpp"
#include "Scenes\Scene.cpp"
#include "Scenes\SceneEnvironmentProbePool.cpp"
#include "Scenes\SceneInput.cpp"
#include "Scenes\SceneInputActions.cpp"
#include "Scenes\ScenePool.cpp"
#include "Scenes\SceneShaderData.cpp"
#include "Scenes\SceneState.cpp"
#elif defined(NETWORKING)
#include "Networking\ASIO.cpp"
#include "Networking\Client.cpp"
#include "Networking\tcp_session_tpl.cpp"
#elif defined(PLATFORM)
#include "Platform\Audio\openAl\ALWrapper.cpp"
#include "Platform\Audio\sdl_mixer\SDLWrapper.cpp"
#include "Platform\Audio\SFXDevice.cpp"
#include "Platform\Compute\OpenCLInterface.cpp"
#include "Libs\simplefilewatcher\source\FileWatcher.cpp"
#include "Libs\simplefilewatcher\source\FileWatcherLinux.cpp"
#include "Libs\simplefilewatcher\source\FileWatcherOSX.cpp"
#include "Libs\simplefilewatcher\source\FileWatcherWin32.cpp"
#include "Platform\File\FileManagementFunctions.cpp"
#include "Platform\File\FileManagementPaths.cpp"
#include "Platform\File\FileUpdateMonitor.cpp"
#include "Platform\File\FileWatcherManager.cpp"
#include "Platform\Input\AutoKeyRepeat.cpp"
#include "Platform\Input\EffectManager.cpp"
#include "Platform\Input\InputHandler.cpp"
#include "Platform\Input\InputAggregatorInterface.cpp"
#include "Platform\Input\InputInterface.cpp"
#include "Platform\Input\JoystickInterface.cpp"
#include "Platform\SDLEventListener.h"
#include "Platform\SDLEventManager.h"
#include "Libs\Allocator\Allocator.cpp"
#include "Libs\Allocator\Fault.cpp"
#include "Libs\Allocator\xallocator.cpp"
#include "Core\MemoryManagement\TrackedObject.cpp"
#include "Libs\Threadpool - c++11\Threadpool.cpp"
#include "Platform\Threading\Task.cpp"
#include "Platform\Threading\ThreadPoolImpl.cpp"
#include "Platform\Video\Buffers\RenderTarget\RenderTarget.cpp"
#include "Platform\Video\Buffers\RenderTarget\RTAttachment.cpp"
#include "Platform\Video\Buffers\RenderTarget\RTAttachmentPool.cpp"
#include "Platform\Video\Buffers\RenderTarget\RTDrawDescriptor.cpp"
#include "Platform\Video\Buffers\ShaderBuffer\ShaderBuffer.cpp"
#include "Platform\Video\Buffers\VertexBuffer\GenericBuffer\AttributeDescriptor.cpp"
#include "Platform\Video\Buffers\VertexBuffer\GenericBuffer\GenericVertexData.cpp"
#include "Platform\Video\Buffers\VertexBuffer\VertexBuffer.cpp"
#include "Platform\Video\Buffers\VertexBuffer\VertexDataInterface.cpp"
#include "Platform\Video\Direct3D\Buffers\PixelBuffer\d3dPixelBuffer.cpp"
#include "Platform\Video\Direct3D\Buffers\RenderTarget\d3dRenderTarget.cpp"
#include "Platform\Video\Direct3D\Buffers\ShaderBuffer\d3dConstantBuffer.cpp"
#include "Platform\Video\Direct3D\Buffers\VertexBuffer\d3dGenericVertexData.cpp"
#include "Platform\Video\Direct3D\Buffers\VertexBuffer\d3dVertexBuffer.cpp"
#include "Platform\Video\Direct3D\Shaders\d3dShaderProgram.cpp"
#include "Platform\Video\Direct3D\Textures\d3dTexture.cpp"
#include "Platform\Video\Direct3D\d3dEnumTable.cpp"
#include "Platform\Video\Direct3D\DXWrapper.cpp"
#include "Platform\Video\OpenGL\Buffers\PixelBuffer\glPixelBuffer.cpp"
#include "Platform\Video\OpenGL\Buffers\RenderTarget\glFramebuffer.cpp"
#include "Platform\Video\OpenGL\Buffers\ShaderBuffer\glUniformBuffer.cpp"
#include "Platform\Video\OpenGL\Buffers\VertexBuffer\glGenericVertexData.cpp"
#include "Platform\Video\OpenGL\Buffers\VertexBuffer\glVAOCache.cpp"
#include "Platform\Video\OpenGL\Buffers\VertexBuffer\glVAOPool.cpp"
#include "Platform\Video\OpenGL\Buffers\VertexBuffer\glVertexArray.cpp"
#include "Platform\Video\OpenGL\Buffers\glBufferImpl.cpp"
#include "Platform\Video\OpenGL\Buffers\glBufferLockManager.cpp"
#include "Platform\Video\OpenGL\Buffers\glGenericBuffer.cpp"
#include "Platform\Video\OpenGL\Buffers\glMemoryManager.cpp"
#include "Libs\GLIM\glimBatch.cpp"
#include "Libs\GLIM\glimBatchAttributes.cpp"
#include "Libs\GLIM\glimBatchData.cpp"
#include "Libs\GLIM\glimD3D11.cpp"
#include "Libs\GLIM\Stuff.cpp"
#include "Platform\Video\OpenGL\glsw\bstrlib.c"
#include "Platform\Video\OpenGL\glsw\glsw.c"
#include "Platform\Video\OpenGL\Shaders\glShader.cpp"
#include "Platform\Video\OpenGL\Shaders\glShaderProgram.cpp"
#include "Platform\Video\OpenGL\Textures\glSamplerOject.cpp"
#include "Platform\Video\OpenGL\Textures\glTexture.cpp"
#include "Platform\Video\OpenGL\GLError.cpp"
#include "Platform\Video\OpenGL\glHardwareQuery.cpp"
#include "Platform\Video\OpenGL\glHardwareQueryPool.cpp"
#include "Platform\Video\OpenGL\glIMPrimitive.cpp"
#include "Platform\Video\OpenGL\glLockManager.cpp"
#include "Platform\Video\OpenGL\glResources.cpp"
#include "Platform\Video\OpenGL\GLStates.cpp"
#include "Platform\Video\OpenGL\GLWrapper.cpp"
#include "Platform\Video\OpenGL\SDLWindowWrapper.cpp"
#include "Libs\RenderDoc - Manager\RenderDocManager.cpp"
#include "Platform\Video\Shaders\ShaderProgram.cpp"
#include "Platform\Video\Textures\Texture.cpp"
#include "Platform\Video\CommandBuffer.cpp"
#include "Platform\Video\CommandBufferPool.cpp"
#include "Platform\Video\GenericCommandPool.cpp"
#include "Platform\Video\GenericDrawCommand.cpp"
#include "Platform\Video\GFXDevice.cpp"
#include "Platform\Video\GFXDeviceDebug.cpp"
#include "Platform\Video\GFXDeviceDraw.cpp"
#include "Platform\Video\GFXDeviceObjects.cpp"
#include "Platform\Video\GFXDeviceState.cpp"
#include "Platform\Video\GFXRTPool.cpp"
#include "Platform\Video\GFXShaderData.cpp"
#include "Platform\Video\GFXState.cpp"
#include "Platform\Video\GraphicsResource.cpp"
#include "Platform\Video\IMPrimitive.cpp"
#include "Platform\Video\Pipeline.cpp"
#include "Platform\Video\PushConstant.cpp"
#include "Platform\Video\PushConstants.cpp"
#include "Platform\Video\RenderAPIWrapper.cpp"
#include "Platform\Video\RenderPackage.cpp"
#include "Platform\Video\RenderStateBlock.cpp"
#include "Platform\Video\TextureData.cpp"
#include "Platform\DisplayWindow.cpp"
#include "Platform\PlatformDefines.cpp"
#include "Platform\PlatformDefinesApple.cpp"
#include "Platform\PlatformDefinesUnix.cpp"
#include "Platform\PlatformDefinesWindows.cpp"
#include "Platform\PlatformRuntime.cpp"
#elif defined(RECAST)
#include "Libs\ReCast\DebugUtils\Source\DebugDraw.cpp"
#include "Libs\ReCast\DebugUtils\Source\DetourDebugDraw.cpp"
#include "Libs\ReCast\DebugUtils\Source\RecastDebugDraw.cpp"
#include "Libs\ReCast\DebugUtils\Source\RecastDump.cpp"
#include "Libs\ReCast\Detour\Source\DetourAlloc.cpp"
#include "Libs\ReCast\Detour\Source\DetourCommon.cpp"
#include "Libs\ReCast\Detour\Source\DetourNavMesh.cpp"
#include "Libs\ReCast\Detour\Source\DetourNavMeshBuilder.cpp"
#include "Libs\ReCast\Detour\Source\DetourNavMeshQuery.cpp"
#include "Libs\ReCast\Detour\Source\DetourNode.cpp"
#include "Libs\ReCast\DetourCrowd\Source\DetourCrowd.cpp"
#include "Libs\ReCast\DetourCrowd\Source\DetourLocalBoundary.cpp"
#include "Libs\ReCast\DetourCrowd\Source\DetourObstacleAvoidance.cpp"
#include "Libs\ReCast\DetourCrowd\Source\DetourPathCorridor.cpp"
#include "Libs\ReCast\DetourCrowd\Source\DetourPathQueue.cpp"
#include "Libs\ReCast\DetourCrowd\Source\DetourProximityGrid.cpp"
#include "Libs\ReCast\DetourTileCache\Source\DetourTileCache.cpp"
#include "Libs\ReCast\DetourTileCache\Source\DetourTileCacheBuilder.cpp"
#include "Libs\ReCast\ReCast\Source\Recast.cpp"
#include "Libs\ReCast\ReCast\Source\RecastAlloc.cpp"
#include "Libs\ReCast\ReCast\Source\RecastArea.cpp"
#include "Libs\ReCast\ReCast\Source\RecastContour.cpp"
#include "Libs\ReCast\ReCast\Source\RecastFilter.cpp"
#include "Libs\ReCast\ReCast\Source\RecastLayers.cpp"
#include "Libs\ReCast\ReCast\Source\RecastMesh.cpp"
#include "Libs\ReCast\ReCast\Source\RecastMeshDetail.cpp"
#include "Libs\ReCast\ReCast\Source\RecastRasterization.cpp"
#include "Libs\ReCast\ReCast\Source\RecastRegion.cpp"
#include "Libs\ReCast\RecastContrib\fastlz\fastlz.c"
#endif