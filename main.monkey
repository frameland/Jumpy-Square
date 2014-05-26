'buildopt: ios
'buildopt: release
Strict
Import vsat
Import source.menu
Import source.particles
#GLFW_WINDOW_WIDTH = 640
#GLFW_WINDOW_HEIGHT = 960


Function Main:Int()
	Vsat = New VsatApp
	Vsat.ChangeScene(New MainMenu)
	Return 0
End
