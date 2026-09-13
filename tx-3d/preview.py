"""Offscreen CAD inspection images, independent of the web viewer."""
import vtk
from assembly import MATERIALS


def render_preview(model,filename,active):
    renderer=vtk.vtkRenderer()
    renderer.SetBackground(.075,.09,.115)
    renderer.SetBackground2(.19,.22,.27)
    renderer.GradientBackgroundOn()
    for p in model.parts:
        if p.group=='fluid' or (active and p.group in ('tank','cover','cooling','accessories','terminals')): continue
        if active and p.name.startswith('base_'): continue
        vertices,faces=p.shape.tessellate(.5,.2)
        points=vtk.vtkPoints()
        for v in vertices: points.InsertNextPoint(v.x+p.position[0],v.y+p.position[1],v.z+p.position[2])
        cells=vtk.vtkCellArray()
        for face in faces:
            cells.InsertNextCell(3)
            for index in face: cells.InsertCellPoint(index)
        poly=vtk.vtkPolyData(); poly.SetPoints(points); poly.SetPolys(cells)
        normals=vtk.vtkPolyDataNormals(); normals.SetInputData(poly); normals.SetFeatureAngle(45); normals.Update()
        mapper=vtk.vtkPolyDataMapper(); mapper.SetInputConnection(normals.GetOutputPort())
        actor=vtk.vtkActor(); actor.SetMapper(mapper)
        material=MATERIALS[p.material]; actor.GetProperty().SetColor(*material['color'][:3])
        actor.GetProperty().SetInterpolationToPhong(); actor.GetProperty().SetSpecular(.35); actor.GetProperty().SetSpecularPower(32)
        renderer.AddActor(actor)
    camera=renderer.GetActiveCamera(); camera.SetPosition(3300,-4400,3100); camera.SetFocalPoint(0,0,1000); camera.SetViewUp(0,0,1)
    camera.ParallelProjectionOn(); renderer.ResetCamera(); camera.Zoom(1.12)
    window=vtk.vtkRenderWindow(); window.SetOffScreenRendering(1); window.SetSize(1500,1200); window.AddRenderer(renderer); window.SetMultiSamples(8); window.Render()
    capture=vtk.vtkWindowToImageFilter(); capture.SetInput(window); capture.SetInputBufferTypeToRGB(); capture.ReadFrontBufferOff(); capture.Update()
    writer=vtk.vtkPNGWriter(); writer.SetFileName(str(filename)); writer.SetInputConnection(capture.GetOutputPort()); writer.Write(); window.Finalize()
