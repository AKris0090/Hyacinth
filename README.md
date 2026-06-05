# 🪻 HYACINTH ![Language](https://img.shields.io/badge/Language-C%2B%2B-blue) ![Vulkan](https://img.shields.io/badge/API-Vulkan-red) ![PhysX](https://img.shields.io/badge/Physics-PhysX-yellow) 
![](readme_images/main.png)  

Hyacinth is a deferred-rendered multiplayer first-person shooter engine made in C++ using Vulkan and PhysX.

# 📽️ Demo 
Hyacinth v0 is out! View some demo clips here: https://www.youtube.com/watch?v=YgR1PEGyKbY

# 🔥 Feature Highlights
## Authoritative Game Server
Multiplayer is enabled through an authoritative game server, engineered based off of netcode from popular FPS titles, including Valorant, Overwatch, and CS:GO. The server handles all communication over UDP, and features:

- multithreading (separating game loop and render loop)
- synchronization & deterministic physics
- entity interpolation
- client prediction / server reconciliation
- lag compensation

For more information on my server architecture, you can view my blog at: https://ajnkrishnan.me/blog-posts/hyacinth-server-architecture.html
## Baked DDGI
This whole project started after I watched this talk from Will Pearce, a graphics engineer on the Overwatch team: https://www.youtube.com/watch?v=0PlxPCq-DbQ. I've always been super interested in the Overwatch rendering style, and this would be a step towards recreating that look.

Overwatch uses a volume-based DDGI approach, where smaller volumes allow for more detailed global illumination. Everything else follows the standard Nvidia implementation of DDGI. To enhance performance of this multi-volume system, Overwatch uses stencil buffers and discard instructions to ensure that the global illumination is only evaluated once per pixel. They use two bits in the stencil buffer pass, a "current" bit, and an "any" bit. GI Evaluation goes something like this:

For each volume:
	1. Draw the volume bounds to the stencil buffer. In the fragment shader, determine if the world space position is inside the volume. If not, discard the pixel.
	2. Volume pass tests against the "any" bit. Sets "current" to 1 if passed.
	3. GI Evaluation pass tests against the "current" bit. If "current" bit is set, "any" is not set. So, invert the "any" bit and "current" bits if passed, and unset both if fail. 
	4. All pixels passing the "current" bit stencil test get written to a GI image that gets composited later on.

This is my stencil pass setup:

```m_volumeStencilPipeline.m_depthStencil.stencilTestEnable = true;
m_volumeStencilPipeline.m_depthStencil.front.compareMask = ANY_BIT;
m_volumeStencilPipeline.m_depthStencil.front.writeMask = CURRENT_BIT;
m_volumeStencilPipeline.m_depthStencil.front.compareOp = VK_COMPARE_OP_EQUAL;
m_volumeStencilPipeline.m_depthStencil.front.reference = 1;
m_volumeStencilPipeline.m_depthStencil.front.passOp = VK_STENCIL_OP_REPLACE;
m_volumeStencilPipeline.m_depthStencil.front.failOp = VK_STENCIL_OP_KEEP;
```

and this is my evaluation pass setup:

```m_ddgiPipelineUtil.m_depthStencil.stencilTestEnable = true;
m_ddgiPipelineUtil.m_depthStencil.front.compareMask = CURRENT_BIT;
m_ddgiPipelineUtil.m_depthStencil.front.writeMask = ANY_BIT | CURRENT_BIT;
m_ddgiPipelineUtil.m_depthStencil.front.compareOp = VK_COMPARE_OP_EQUAL;
m_ddgiPipelineUtil.m_depthStencil.front.reference = 1;
m_ddgiPipelineUtil.m_depthStencil.front.passOp = VK_STENCIL_OP_INVERT;
m_ddgiPipelineUtil.m_depthStencil.front.failOp = VK_STENCIL_OP_ZERO;
```

You can see below, with a small volume enclosing only the top windowed corridor of sponza.

|                                   stencil pass                           |                     ddgi evaluation                                      |
| :----------------------------------------------------------------------: | :----------------------------------------------------------------------: |
|                   ![](readme_images/stencil.png)                         |                         ![](readme_images/dd.png)                        |

|                                   full ddgi eval                         |                         final color                                      |
| :----------------------------------------------------------------------: | :----------------------------------------------------------------------: |
|                   ![](readme_images/ddgi.png)                            |                         ![](readme_images/final.png)                     |
