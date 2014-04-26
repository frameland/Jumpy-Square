Strict
Import vsat
Import particles


Class ParticleBackground
	
	Field emitter:ParticleEmitter
	Field lastFrame:Int
	
	Method New()
		Local baseUnit:Float = Vsat.ScreenWidth2
		emitter = New ParticleEmitter
		emitter.InitWithSize(120)
		emitter.particleLifeSpan = 15.0
		emitter.particleLifeSpanVariance = 5.0
		emitter.emissionRate = 6
		
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 1.0)
		emitter.positionVariance.Set(baseUnit, baseUnit * 0.5)
		emitter.size.Set(0, 0)
		emitter.endSize.Set(baseUnit/12, baseUnit/12)
		emitter.endSizeVariance.Set(baseUnit*0.1, baseUnit*0.1)

		emitter.startColor.Alpha = 0.1
		emitter.endColor.Alpha = 0.0
		emitter.emissionAngle = -90
		emitter.speed = 45
		emitter.speedVariance = 25
		
		emitter.Start()
		emitter.FastForward(20, 0.01666)
	End
	
	Method Update:Void(dt:Float)
		If lastFrame = Vsat.Frame
			Return
		End
		lastFrame = Vsat.Frame
		emitter.Update(dt)
	End
	
	Method Render:Void()
		emitter.Render()
	End
	
End
