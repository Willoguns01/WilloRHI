Handles a number of core systems
- creating and destroying resources and objects
- memory management/memory pools
- handling of descriptors
- queue submission
- swapchain image acquisition and present

Creating resources and objects:
- [[Image]], [[Buffer]], [[Sampler]], [[Swapchain]]
- some create* functions return a handle instead of the actual object
	- [[ImageHandle]], [[BufferHandle]], [[SamplerHandle]]
	- handle contains index into list of that resource
	- each resource (depends) contains descriptor set index or device address

Destroying resources and objects:
- prior to running (or prior to rendering the related frame), set used swapchain via device function
- device uses set swapchain's timeline semaphore to track resource usage
- "destroy" function queues Zombie* object (i.e. [[ZombieImage]])
	- set zombie NumSwapchainWait to number of swapchains managed by device
	- set 

Descriptor management:
- all images, samplers and buffers are indexed globally via one big descriptor set
- updateAfterBind enabled selectively (can be set by client application)
- create 2^20 descriptors for each type (around 1mil)
- allow some kind of interface for overwriting image handles' descriptors with that of another handle (update after bind)
