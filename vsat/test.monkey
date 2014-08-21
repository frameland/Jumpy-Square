'buildopt: run
Import vsat
Import mojo

Function Main:Int()
	Vsat = New VsatApp
	Vsat.displayFps = True
	Vsat.ChangeScene(New vvv)
	Return 0
End

Class vvv Extends VScene Implements VActionEventHandler
	Field x:Int
	Field shapes:List<VShape> = New List<VShape>
	Field actions:List<VAction> = New List<VAction>
	
	Method OnInit:Void()
		Local fade:= New VFadeInLinear(0.5)
		fade.SetColor(Color.Teal)
		Vsat.StartFadeIn(fade)
		
		Local a:= New VCircle(100, 100, 30)
		a.renderOutline = True
		Local b:= New VRect(300, 200, 20, 100)
		shapes.AddLast(a)
		shapes.AddLast(b)
		
	End
	
	Method OnUpdate:Void(dt:Float)
		x += 1
		If KeyHit(KEY_RIGHT)
			Local move:= New VVec2Action(shapes.First().scale, 1.0, 1.0, 1.0, EASE_OUT_BACK)
			move.AddToList(actions)
			move.SetListener(Self)
		End
		
		For Local a:= EachIn actions
			a.Update(dt)
		Next
	End
	
	
	Method OnRender:Void()
		ClearScreenWithColor(Color.Navy)
		For Local a:= EachIn shapes
			a.Render()
		Next
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		Select id
			Case VAction.STARTED
				Print "Start"
			Case VAction.FINISHED
				Print "Done"
				
		End
	End
	
End

