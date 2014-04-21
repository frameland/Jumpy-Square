Strict
Import vsat

Class Enemy Extends VRect
	
	Field velocity:Vec2
	Field gravity:Float = Vsat.ScreenHeight / 38
	Field link:ListNode<Enemy>
	Field widthRelative:Float = 9.6
	
'--------------------------------------------------------------------------
' * helpers for medals, etc.
'--------------------------------------------------------------------------
	Field accountsForPoints:Bool = True
	Field hasBeenScored:Bool
	Field wasClose:Bool
	Field isSurprise:Bool
	

'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method New()
		Super.New(0, -Vsat.ScreenHeight/widthRelative, Vsat.ScreenHeight/widthRelative, Vsat.ScreenHeight/widthRelative)
		color.Set(Color.NewRed)
		color.Alpha = 0.0
		renderOutline = True
		velocity = New Vec2
	End
	
	Method SetLeft:Void()
		position.x = 1
	End
	
	Method SetRight:Void()
		position.x = Vsat.ScreenWidth - Self.size.x - 1
	End
	
	Method Remove:Void()
		If link Then link.Remove()
	End
	
	
'--------------------------------------------------------------------------
' * Update & Render
'--------------------------------------------------------------------------	
	Method UpdatePhysics:Void(dt:Float)
		If color.Alpha < 1
			color.Alpha += 2 * dt
		End
		
		velocity.y += gravity
		velocity.Limit(gravity * 50)
		position.Add(velocity.x * dt, velocity.y * dt)
		If position.y > Vsat.ScreenHeight
			Self.Remove()
			If accountsForPoints
				Local ev:= New VEvent
				ev.id = "RemoveEnemy"
				Vsat.FireEvent(ev)
			End
		End
	End
	
	Method DrawOutline:Void()
		DrawRectOutline(0, 0, size.x, size.y)
		DrawRectOutline(-1, -1, size.x+2, size.y+2)
	End

	
End
