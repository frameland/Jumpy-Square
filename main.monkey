'buildopt: run
Strict
Import vsat
Import source.menu
Import source.game

Function Main:Int()
	Vsat = New VsatApp
	Vsat.ChangeScene(New MainMenu)
	Return 0
End
