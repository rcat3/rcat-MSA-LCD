// The thread's mesh. It's in a file of its own, brought in with `use`, so the
// path below is found relative to this folder whichever housing includes
// voice_port.scad.
module vpu_thread_mesh() import("vendor/vpu_thread.stl", convexity = 10);
